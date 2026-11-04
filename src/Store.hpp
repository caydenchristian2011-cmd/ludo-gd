#pragma once
#include <Geode/Geode.hpp>
#include <string>
#include <vector>

namespace ludo {

struct LevelStat {
    int id = 0;
    std::string name;
    std::string creator;
    int attempts = 0;
    int deaths = 0;
    int completions = 0;
    int best = 0;            // best normal-mode percent
    double seconds = 0;      // time spent inside the level
    long long last = 0;      // unix seconds, last time it was played
    bool favorite = false;
};

struct Collection {
    std::string name;
    std::vector<int> ids;
};

// Everything Ludo remembers. Lives in the mod's save folder as stats.json.
class Store {
public:
    static Store& get();

    LevelStat& level(int id, std::string const& name, std::string const& creator);
    LevelStat* find(int id);
    std::vector<LevelStat*> byRecent();
    std::vector<LevelStat*> byTime();

    std::vector<Collection>& collections() { return m_collections; }

    double totalSeconds() const;
    int totalAttempts() const;
    int totalDeaths() const;
    int totalCompletions() const;
    size_t levelCount() const { return m_levels.size(); }

    void dirty() { m_dirty = true; }
    void saveIfDirty();

private:
    Store();
    void load();
    void save();

    std::vector<LevelStat> m_levels;
    std::vector<Collection> m_collections;
    bool m_dirty = false;
};

std::string formatDuration(double seconds);

}
