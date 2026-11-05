#include "Drill.hpp"
#include "Store.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>

using namespace geode::prelude;

namespace ludo {

static constexpr int kGap[] = {1, 3, 7, 14};   // days until the next review, by stage reached

float Spot::startPercent() const {
    return std::max(0.f, static_cast<float>(from) * 5.f - 3.f);
}

std::string Spot::label() const {
    return fmt::format("{}-{}%", from * 5, to * 5);
}

Drill& Drill::get() {
    static Drill d;
    return d;
}

Drill::Drill() { load(); }

// Days since 1970-01-01 for the local calendar date (no time zone tables needed).
long long Drill::epochDay() {
    std::time_t t = std::time(nullptr);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    long long y = lt.tm_year + 1900, m = lt.tm_mon + 1, d = lt.tm_mday;
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void Drill::refresh() {
    constexpr int kMinDeaths = 8;        // below this the history says nothing yet
    constexpr int kMaxSpotsPerLevel = 3;
    constexpr int kMaxSlices = 4;        // a spot is at most 20% of a level
    long long today = epochDay();

    for (auto* l : Store::get().byRecent()) {
        int total = 0;
        for (int h : l->hist) total += h;
        if (total < kMinDeaths) continue;

        bool active = false;
        for (auto& s : m_spots) if (s.levelId == l->id && !s.mastered) { active = true; break; }
        if (active) continue;

        // a slice is "hot" when it holds well over its fair share of deaths
        bool hot[20];
        for (int i = 0; i < 20; i++) hot[i] = l->hist[i] >= 3 && l->hist[i] * 20 >= total * 16 / 10;

        struct Run { int from, to, deaths; };
        std::vector<Run> runs;
        for (int i = 0; i < 20;) {
            if (!hot[i]) { i++; continue; }
            int j = i, sum = 0;
            while (j < 20 && hot[j]) sum += l->hist[j++];
            // long runs are cut to their worst window
            if (j - i > kMaxSlices) {
                int bestStart = i, bestSum = -1;
                for (int a = i; a + kMaxSlices <= j; a++) {
                    int w = 0;
                    for (int k = a; k < a + kMaxSlices; k++) w += l->hist[k];
                    if (w > bestSum) { bestSum = w; bestStart = a; }
                }
                runs.push_back({bestStart, bestStart + kMaxSlices, bestSum});
            } else {
                runs.push_back({i, j, sum});
            }
            i = j;
        }
        std::sort(runs.begin(), runs.end(), [](Run const& a, Run const& b) { return a.deaths > b.deaths; });
        if (runs.size() > kMaxSpotsPerLevel) runs.resize(kMaxSpotsPerLevel);
        for (auto& r : runs) {
            Spot s;
            s.levelId = l->id;
            s.from = r.from;
            s.to = r.to;
            s.due = today;
            m_spots.push_back(s);
            m_dirty = true;
        }
    }
    if (m_dirty) save();
}

std::vector<Spot*> Drill::dueSpots() {
    long long today = epochDay();
    std::vector<Spot*> out;
    for (auto& s : m_spots) if (!s.mastered && s.due <= today) out.push_back(&s);
    std::sort(out.begin(), out.end(), [](Spot* a, Spot* b) { return a->due < b->due; });
    return out;
}

int Drill::dueCount() { return static_cast<int>(dueSpots().size()); }

Spot* Drill::nextDue(int preferLevel) {
    auto due = dueSpots();
    for (auto* s : due) if (s->levelId == preferLevel) return s;
    return due.empty() ? nullptr : due.front();
}

Spot* Drill::current() {
    if (!m_session.active) return nullptr;
    for (auto& s : m_spots)
        if (!s.mastered && s.levelId == m_session.levelId && s.from == m_session.from && s.to == m_session.to) return &s;
    return nullptr;
}

bool Drill::onPass(Spot& s) {
    s.passes++;
    s.clean++;
    bool advanced = false;
    if (s.clean >= kCleanToAdvance) {
        advanced = true;
        s.clean = 0;
        s.stage++;
        if (s.stage > kStages) {
            s.mastered = true;
        } else {
            s.due = epochDay() + kGap[std::min(s.stage, kStages) - 1];
        }
        m_session.cleared++;
    }
    m_dirty = true;
    save();
    return advanced;
}

void Drill::onDeath(Spot& s) {
    s.deaths++;
    s.clean = 0;
    m_dirty = true;
}

void Drill::noteY(int levelId, int percent, float y) {
    if (percent < 0 || percent > 100) return;
    auto it = m_trail.find(levelId);
    if (it == m_trail.end()) it = m_trail.emplace(levelId, std::array<float, 101>{}).first;
    if (std::abs(it->second[static_cast<size_t>(percent)] - y) < 1.f) return;
    it->second[static_cast<size_t>(percent)] = y;
    m_dirty = true;
}

float Drill::yAt(int levelId, float percent, float fallback, bool* found) const {
    if (found) *found = false;
    auto it = m_trail.find(levelId);
    if (it == m_trail.end()) return fallback;
    int p = std::clamp(static_cast<int>(percent + 0.5f), 0, 100);
    for (int d = 0; d <= 3; d++) {   // nearest known point within 3%
        for (int sign : {-1, 1}) {
            int q = p + sign * d;
            if (q < 0 || q > 100) continue;
            float y = it->second[static_cast<size_t>(q)];
            if (y > 0.f) { if (found) *found = true; return y; }
        }
    }
    return fallback;
}

void Drill::save() {
    std::string out = "{\"v\":1,\"spots\":[";
    bool first = true;
    for (auto& s : m_spots) {
        if (!first) out += ",";
        first = false;
        out += fmt::format("{{\"level\":{},\"from\":{},\"to\":{},\"stage\":{},\"clean\":{},\"due\":{},\"passes\":{},\"deaths\":{},\"mastered\":{}}}",
            s.levelId, s.from, s.to, s.stage, s.clean, s.due, s.passes, s.deaths, s.mastered ? "true" : "false");
    }
    out += "],\"trail\":[";
    first = true;
    for (auto& [id, ys] : m_trail) {
        if (!first) out += ",";
        first = false;
        out += fmt::format("{{\"level\":{},\"y\":[", id);
        for (size_t i = 0; i < ys.size(); i++) out += fmt::format("{}{:.0f}", i ? "," : "", ys[i]);
        out += "]}";
    }
    out += "]}";
    auto res = utils::file::writeStringSafe(Mod::get()->getSaveDir() / "drill.json", out);
    if (res.isOk()) m_dirty = false;
    else log::warn("Ludo could not save the drill file");
}

void Drill::load() {
    auto text = utils::file::readString(Mod::get()->getSaveDir() / "drill.json");
    if (!text) return;
    auto parsed = matjson::parse(text.unwrap());
    if (!parsed) { log::warn("Ludo: drill.json could not be read, starting fresh"); return; }
    auto root = parsed.unwrap();
    for (auto const& item : root["trail"]) {
        int id = static_cast<int>(item["level"].asInt().unwrapOr(0));
        if (id <= 0) continue;
        std::array<float, 101> ys{};
        size_t i = 0;
        for (auto const& v : item["y"]) { if (i >= ys.size()) break; ys[i++] = static_cast<float>(v.asDouble().unwrapOr(0.0)); }
        m_trail[id] = ys;
    }
    for (auto const& item : root["spots"]) {
        Spot s;
        s.levelId = static_cast<int>(item["level"].asInt().unwrapOr(0));
        s.from = std::clamp(static_cast<int>(item["from"].asInt().unwrapOr(0)), 0, 19);
        s.to = std::clamp(static_cast<int>(item["to"].asInt().unwrapOr(1)), s.from + 1, 20);
        s.stage = static_cast<int>(item["stage"].asInt().unwrapOr(0));
        s.clean = static_cast<int>(item["clean"].asInt().unwrapOr(0));
        s.due = static_cast<long long>(item["due"].asInt().unwrapOr(0));
        s.passes = static_cast<int>(item["passes"].asInt().unwrapOr(0));
        s.deaths = static_cast<int>(item["deaths"].asInt().unwrapOr(0));
        s.mastered = item["mastered"].asBool().unwrapOr(false);
        if (s.levelId > 0) m_spots.push_back(s);
    }
}

}
