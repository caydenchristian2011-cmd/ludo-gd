#pragma once
#include <Geode/Geode.hpp>
#include <array>
#include <map>
#include <string>
#include <vector>

namespace ludo {

// A stretch of a level where you keep dying, in 5% slices: [from, to).
struct Spot {
    int levelId = 0;
    int from = 0;          // first slice, 0..19
    int to = 1;            // one past the last slice, 1..20
    int stage = 0;         // 0..4, how far up the review ladder it is
    int clean = 0;         // clean passes in a row at the current stage
    long long due = 0;     // epoch day this spot is next due
    int passes = 0;        // lifetime clean passes
    int deaths = 0;        // deaths while drilling (kept apart from the level's own history)
    bool mastered = false;

    float startPercent() const;   // where the practice start pos goes (a little run-up before the spot)
    float endPercent() const { return static_cast<float>(to) * 5.f; }
    std::string label() const;    // "35-50%"
};

// Spaced-repetition practice on the places you die most. Keeps its own file, drill.json.
class Drill {
public:
    static Drill& get();

    static long long epochDay();   // local calendar day number
    static constexpr int kCleanToAdvance = 3;
    static constexpr int kStages = 4;   // review gaps of 1, 3, 7 and 14 days

    // Looks at every level's death history and adds spots for levels that have none active.
    void refresh();
    std::vector<Spot>& spots() { return m_spots; }
    std::vector<Spot*> dueSpots();                 // due today, most urgent first
    int dueCount();
    Spot* nextDue(int preferLevel);                // prefers the given level so a session stays put

    bool onPass(Spot& s);    // one clean run of the spot; true when it moved up a stage
    void onDeath(Spot& s);   // a death inside the spot breaks the streak
    void save();
    void saveIfDirty() { if (m_dirty) save(); }

    // Where the player actually was, per 1% of each level. A start pos is placed at that height,
    // because the portals alone do not say how high a ship or ufo is flying there.
    void noteY(int levelId, int percent, float y);
    float yAt(int levelId, float percent, float fallback, bool* found = nullptr) const;

    // The live session (not saved).
    struct Session {
        bool active = false;
        int levelId = 0;
        int from = 0, to = 1;
        int cleared = 0;     // spots finished this session
    };
    Session& session() { return m_session; }
    bool drilling(int levelId) const { return m_session.active && m_session.levelId == levelId; }
    Spot* current();

private:
    Drill();
    void load();

    std::vector<Spot> m_spots;
    std::map<int, std::array<float, 101>> m_trail;
    Session m_session;
    bool m_dirty = false;
};

// Starts a drill session on a spot: opens the level at the spot.
bool startDrill(Spot& spot);

}
