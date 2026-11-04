#include "Store.hpp"
#include <algorithm>
#include <ctime>

using namespace geode::prelude;

namespace ludo {

static std::string esc(std::string const& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) out += ' ';
                else out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

static std::string histText(std::array<int, 20> const& h) {
    std::string out;
    for (size_t i = 0; i < h.size(); i++) out += (i ? "," : "") + std::to_string(h[i]);
    return out;
}

Store& Store::get() {
    static Store inst;
    return inst;
}

Store::Store() { load(); }

LevelStat& Store::level(int id, std::string const& name, std::string const& creator) {
    for (auto& l : m_levels) {
        if (l.id == id) {
            if (!name.empty() && l.name != name) { l.name = name; m_dirty = true; }
            if (!creator.empty() && l.creator != creator) { l.creator = creator; m_dirty = true; }
            return l;
        }
    }
    LevelStat s;
    s.id = id;
    s.name = name;
    s.creator = creator;
    m_levels.push_back(std::move(s));
    m_dirty = true;
    return m_levels.back();
}

LevelStat* Store::find(int id) {
    for (auto& l : m_levels) if (l.id == id) return &l;
    return nullptr;
}

std::vector<LevelStat*> Store::byRecent() {
    std::vector<LevelStat*> out;
    for (auto& l : m_levels) out.push_back(&l);
    std::sort(out.begin(), out.end(), [](LevelStat* a, LevelStat* b) { return a->last > b->last; });
    return out;
}

std::vector<LevelStat*> Store::byTime() {
    std::vector<LevelStat*> out;
    for (auto& l : m_levels) out.push_back(&l);
    std::sort(out.begin(), out.end(), [](LevelStat* a, LevelStat* b) { return a->seconds > b->seconds; });
    return out;
}

static std::string localDate(std::tm const& t) {
    return fmt::format("{:04}-{:02}-{:02}", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

DayStat& Store::today() {
    std::time_t now = std::time(nullptr);
    std::tm t{};
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    std::string date = localDate(t);
    if (m_days.empty() || m_days.back().date != date) {
        DayStat d;
        d.date = date;
        m_days.push_back(std::move(d));
    }
    return m_days.back();
}

DayLevel& Store::todayLevel(int id) {
    auto& d = today();
    for (auto& l : d.levels) if (l.id == id) return l;
    DayLevel l;
    l.id = id;
    d.levels.push_back(std::move(l));
    return d.levels.back();
}

void Store::logTime(int id, double seconds) {
    if (seconds <= 0) return;
    auto& d = today();
    d.seconds += seconds;
    // time played late at night (feeds the "Night Owl" title)
    std::time_t now = std::time(nullptr);
    std::tm t{};
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    if (t.tm_hour >= 22 || t.tm_hour < 5) d.night += seconds;
    todayLevel(id).seconds += seconds;
    m_dirty = true;
}

void Store::logAttempt(int id) {
    (void)id;
    today().attempts++;
    m_dirty = true;
}

void Store::logDeath(int id, int slice) {
    today().deaths++;
    auto& l = todayLevel(id);
    l.deaths++;
    l.hist[static_cast<size_t>(std::clamp(slice, 0, 19))]++;
    m_dirty = true;
}

void Store::logComplete(int id) {
    today().completions++;
    todayLevel(id).completions++;
    m_dirty = true;
}

double Store::totalSeconds() const { double t = 0; for (auto& l : m_levels) t += l.seconds; return t; }
int Store::totalAttempts() const { int t = 0; for (auto& l : m_levels) t += l.attempts; return t; }
int Store::totalDeaths() const { int t = 0; for (auto& l : m_levels) t += l.deaths; return t; }
int Store::totalBeaten() const { int t = 0; for (auto& l : m_levels) if (l.completions > 0) t++; return t; }
int Store::totalCompletions() const { int t = 0; for (auto& l : m_levels) t += l.completions; return t; }

bool Store::isFavorite(int id) {
    auto* s = find(id);
    return s && s->favorite;
}

bool Store::toggleFavorite(int id, std::string const& name, std::string const& creator) {
    auto& s = level(id, name, creator);
    s.favorite = !s.favorite;
    m_dirty = true;
    saveIfDirty();
    return s.favorite;
}

size_t Store::createCollection(std::string name) {
    if (name.size() > 40) name.resize(40);
    Collection c;
    c.name = std::move(name);
    m_collections.push_back(std::move(c));
    m_dirty = true;
    saveIfDirty();
    return m_collections.size() - 1;
}

void Store::deleteCollection(size_t index) {
    if (index >= m_collections.size()) return;
    m_collections.erase(m_collections.begin() + static_cast<std::ptrdiff_t>(index));
    m_dirty = true;
    saveIfDirty();
}

bool Store::inCollection(size_t index, int id) const {
    if (index >= m_collections.size()) return false;
    for (int x : m_collections[index].ids) if (x == id) return true;
    return false;
}

void Store::addToCollection(size_t index, int id) {
    if (index >= m_collections.size() || inCollection(index, id)) return;
    m_collections[index].ids.push_back(id);
    m_dirty = true;
    saveIfDirty();
}

void Store::removeFromCollection(size_t index, int id) {
    if (index >= m_collections.size()) return;
    auto& ids = m_collections[index].ids;
    ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
    m_dirty = true;
    saveIfDirty();
}

void Store::saveIfDirty() {
    if (m_dirty) save();
}

void Store::save() {
    std::string out = "{\"v\":2,\"levels\":[";
    bool first = true;
    for (auto& l : m_levels) {
        if (!first) out += ",";
        first = false;
        out += fmt::format(
            "{{\"id\":{},\"name\":{},\"creator\":{},\"attempts\":{},\"deaths\":{},\"completions\":{},\"best\":{},\"seconds\":{:.2f},\"last\":{},\"favorite\":{},\"hist\":[{}],\"song\":{},\"track\":{}}}",
            l.id, esc(l.name), esc(l.creator), l.attempts, l.deaths, l.completions, l.best, l.seconds, l.last, l.favorite ? "true" : "false", histText(l.hist), l.song, l.track);
    }
    out += "],\"collections\":[";
    first = true;
    for (auto& c : m_collections) {
        if (!first) out += ",";
        first = false;
        out += "{\"name\":" + esc(c.name) + ",\"ids\":[";
        for (size_t i = 0; i < c.ids.size(); i++) out += (i ? "," : "") + std::to_string(c.ids[i]);
        out += "]}";
    }
    out += "],\"days\":[";
    first = true;
    for (auto& d : m_days) {
        if (!first) out += ",";
        first = false;
        out += fmt::format("{{\"date\":{},\"seconds\":{:.1f},\"night\":{:.1f},\"attempts\":{},\"deaths\":{},\"completions\":{},\"levels\":[",
            esc(d.date), d.seconds, d.night, d.attempts, d.deaths, d.completions);
        bool f2 = true;
        for (auto& l : d.levels) {
            if (!f2) out += ",";
            f2 = false;
            out += fmt::format("{{\"id\":{},\"seconds\":{:.1f},\"deaths\":{},\"completions\":{},\"hist\":[{}]}}",
                l.id, l.seconds, l.deaths, l.completions, histText(l.hist));
        }
        out += "]}";
    }
    out += "]}";

    auto path = Mod::get()->getSaveDir() / "stats.json";
    // writeStringSafe writes a temporary file first, so a crash never leaves half a file
    auto res = utils::file::writeStringSafe(path, out);
    if (res.isOk()) m_dirty = false;
    else log::warn("Ludo could not save its stats");
}

void Store::load() {
    auto path = Mod::get()->getSaveDir() / "stats.json";
    auto text = utils::file::readString(path);
    if (!text) return;
    auto parsed = matjson::parse(text.unwrap());
    if (!parsed) { log::warn("Ludo: stats.json could not be read, starting fresh"); return; }
    auto root = parsed.unwrap();

    for (auto const& item : root["levels"]) {
        LevelStat l;
        l.id = static_cast<int>(item["id"].asInt().unwrapOr(0));
        if (l.id <= 0) continue;
        l.name = item["name"].asString().unwrapOr("");
        l.creator = item["creator"].asString().unwrapOr("");
        l.attempts = static_cast<int>(item["attempts"].asInt().unwrapOr(0));
        l.deaths = static_cast<int>(item["deaths"].asInt().unwrapOr(0));
        l.completions = static_cast<int>(item["completions"].asInt().unwrapOr(0));
        l.best = static_cast<int>(item["best"].asInt().unwrapOr(0));
        l.seconds = item["seconds"].asDouble().unwrapOr(0.0);
        l.last = static_cast<long long>(item["last"].asInt().unwrapOr(0));
        l.favorite = item["favorite"].asBool().unwrapOr(false);
        size_t slot = 0;
        for (auto const& h : item["hist"]) {
            if (slot >= l.hist.size()) break;
            l.hist[slot++] = static_cast<int>(h.asInt().unwrapOr(0));
        }
        l.song = static_cast<int>(item["song"].asInt().unwrapOr(0));
        l.track = static_cast<int>(item["track"].asInt().unwrapOr(0));
        m_levels.push_back(std::move(l));
    }
    for (auto const& item : root["days"]) {
        DayStat d;
        d.date = item["date"].asString().unwrapOr("");
        if (d.date.size() != 10) continue;
        d.seconds = item["seconds"].asDouble().unwrapOr(0.0);
        d.night = item["night"].asDouble().unwrapOr(0.0);
        d.attempts = static_cast<int>(item["attempts"].asInt().unwrapOr(0));
        d.deaths = static_cast<int>(item["deaths"].asInt().unwrapOr(0));
        d.completions = static_cast<int>(item["completions"].asInt().unwrapOr(0));
        for (auto const& li : item["levels"]) {
            DayLevel l;
            l.id = static_cast<int>(li["id"].asInt().unwrapOr(0));
            l.seconds = li["seconds"].asDouble().unwrapOr(0.0);
            l.deaths = static_cast<int>(li["deaths"].asInt().unwrapOr(0));
            l.completions = static_cast<int>(li["completions"].asInt().unwrapOr(0));
            size_t slot = 0;
            for (auto const& h : li["hist"]) {
                if (slot >= l.hist.size()) break;
                l.hist[slot++] = static_cast<int>(h.asInt().unwrapOr(0));
            }
            d.levels.push_back(std::move(l));
        }
        m_days.push_back(std::move(d));
    }
    for (auto const& item : root["collections"]) {
        Collection c;
        c.name = item["name"].asString().unwrapOr("");
        if (c.name.empty()) continue;
        for (auto const& id : item["ids"]) c.ids.push_back(static_cast<int>(id.asInt().unwrapOr(0)));
        m_collections.push_back(std::move(c));
    }
}

std::string formatDuration(double seconds) {
    long long s = static_cast<long long>(seconds + 0.5);
    long long h = s / 3600, m = (s % 3600) / 60;
    if (h > 0) return fmt::format("{}h {:02}m", h, m);
    if (m > 0) return fmt::format("{}m", m);
    return fmt::format("{}s", s);
}

}
