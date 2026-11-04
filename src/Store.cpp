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

double Store::totalSeconds() const { double t = 0; for (auto& l : m_levels) t += l.seconds; return t; }
int Store::totalAttempts() const { int t = 0; for (auto& l : m_levels) t += l.attempts; return t; }
int Store::totalDeaths() const { int t = 0; for (auto& l : m_levels) t += l.deaths; return t; }
int Store::totalCompletions() const { int t = 0; for (auto& l : m_levels) t += l.completions; return t; }

void Store::saveIfDirty() {
    if (m_dirty) save();
}

void Store::save() {
    std::string out = "{\"v\":1,\"levels\":[";
    bool first = true;
    for (auto& l : m_levels) {
        if (!first) out += ",";
        first = false;
        out += fmt::format(
            "{{\"id\":{},\"name\":{},\"creator\":{},\"attempts\":{},\"deaths\":{},\"completions\":{},\"best\":{},\"seconds\":{:.2f},\"last\":{},\"favorite\":{}}}",
            l.id, esc(l.name), esc(l.creator), l.attempts, l.deaths, l.completions, l.best, l.seconds, l.last, l.favorite ? "true" : "false");
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
    out += "]}";

    auto dir = Mod::get()->getSaveDir();
    auto tmp = dir / "stats.json.tmp";
    auto dest = dir / "stats.json";
    if (utils::file::writeString(tmp, out).isOk()) {
        std::error_code ec;
        std::filesystem::rename(tmp, dest, ec);
        if (ec) log::warn("Ludo could not save its stats: {}", ec.message());
        else m_dirty = false;
    }
}

void Store::load() {
    auto path = Mod::get()->getSaveDir() / "stats.json";
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return;
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
        m_levels.push_back(std::move(l));
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
