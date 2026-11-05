// Runs a drill inside the game: opens the level, drops a start pos at the weak spot with the
// right gamemode and speed, counts clean passes, and loops the spot until it is cleared.
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <algorithm>
#include "Drill.hpp"

using namespace geode::prelude;

namespace ludo {

bool startDrill(Spot& spot) {
    auto* level = GameLevelManager::get()->getSavedLevel(spot.levelId);
    if (!level) {
        Notification::create("Open this level once in the game first, then try again", NotificationIcon::Warning)->show();
        return false;
    }
    auto& sess = Drill::get().session();
    sess.active = true;
    sess.levelId = spot.levelId;
    sess.from = spot.from;
    sess.to = spot.to;
    sess.cleared = 0;
    auto scene = PlayLayer::scene(level, false, false);
    CCDirector::get()->pushScene(CCTransitionFade::create(0.4f, scene));
    return true;
}

}

namespace {
// What the player looks like at a point in a level: what the portals before it have set.
struct StartState {
    int mode = 0;               // 0 cube, 1 ship, 2 ball, 3 ufo, 4 wave, 5 robot, 6 spider, 7 swing
    Speed speed = Speed::Normal;
    bool mini = false;
    bool dual = false;
    bool mirror = false;
    bool flipped = false;       // upside-down gravity
    float y = 105.f;            // about the ground line for a cube
};

int modeFor(int id) {
    switch (id) {
        case 12: return 0;
        case 13: return 1;
        case 47: return 2;
        case 111: return 3;
        case 660: return 4;
        case 745: return 5;
        case 1331: return 6;
        case 1933: return 7;
        default: return -1;
    }
}
}

class $modify(LudoDrillLayer, PlayLayer) {
    struct Fields {
        StartPosObject* startPos = nullptr;
        bool drilling = false;
        bool placed = false;
    };

    StartState stateAt(float x) {
        struct Hit { float x; int id; float y; };
        std::vector<Hit> hits;
        for (auto* obj : CCArrayExt<GameObject*>(m_objects)) {
            if (!obj) continue;
            int id = obj->m_objectID;
            bool interesting = modeFor(id) >= 0 || (id >= 200 && id <= 203) || id == 1334 || id == 99 || id == 101 ||
                               id == 286 || id == 287 || id == 45 || id == 46 || id == 10 || id == 11;
            if (!interesting) continue;
            float ox = obj->getPositionX();
            if (ox <= x) hits.push_back({ox, id, obj->getPositionY()});
        }
        std::sort(hits.begin(), hits.end(), [](Hit const& a, Hit const& b) { return a.x < b.x; });

        StartState st;
        if (m_levelSettings) {
            st.mode = m_levelSettings->m_startMode;
            st.speed = m_levelSettings->m_startSpeed;
            st.mini = m_levelSettings->m_startMini;
            st.dual = m_levelSettings->m_startDual;
            st.mirror = m_levelSettings->m_mirrorMode;
        }
        for (auto& h : hits) {
            int m = modeFor(h.id);
            if (m >= 0) { st.mode = m; st.y = h.y; continue; }
            switch (h.id) {
                case 200: st.speed = Speed::Slow; break;
                case 201: st.speed = Speed::Normal; break;
                case 202: st.speed = Speed::Fast; break;
                case 203: st.speed = Speed::Faster; break;
                case 1334: st.speed = Speed::Fastest; break;
                case 99: st.mini = false; break;
                case 101: st.mini = true; break;
                case 286: st.dual = true; break;
                case 287: st.dual = false; break;
                case 45: st.mirror = true; break;
                case 46: st.mirror = false; break;
                case 10: st.flipped = false; break;
                case 11: st.flipped = true; break;
                default: break;
            }
        }
        return st;
    }

    void placeStart(ludo::Spot const& spot) {
        float x = spot.startPercent() / 100.f * m_levelLength;
        auto st = this->stateAt(x);
        bool known = false;
        st.y = ludo::Drill::get().yAt(spot.levelId, spot.startPercent(), st.y, &known);
        if (!known && st.mode != 0) {
            Notification::create("Play this level past " + fmt::format("{}%", static_cast<int>(spot.startPercent())) + " once so Drill learns the height", NotificationIcon::Info, 3.f)->show();
        }
        log::info("Ludo drill: height {:.0f} ({})", st.y, known ? "from your own runs" : "guessed from portals");
        log::info("Ludo drill: level {} spot {} -> x={:.0f} y={:.0f} mode={} speed={} mini={} dual={} mirror={} flipped={}",
            spot.levelId, spot.label(), x, st.y, st.mode, static_cast<int>(st.speed), st.mini, st.dual, st.mirror, st.flipped);

        // copy the level's own start settings (colours, background) and change only what the portals changed
        LevelSettingsObject* settings = nullptr;
        if (m_levelSettings) settings = LevelSettingsObject::objectFromString(m_levelSettings->getSaveString());
        if (!settings) settings = LevelSettingsObject::create();
        settings->m_startMode = st.mode;
        settings->m_startSpeed = st.speed;
        settings->m_startMini = st.mini;
        settings->m_startDual = st.dual;
        settings->m_mirrorMode = st.mirror;
        settings->m_isFlipped = st.flipped;
        settings->retain();

        auto* sp = StartPosObject::create();
        sp->retain();
        sp->m_startSettings = settings;
        sp->setStartPos({x, st.y});
        sp->setPosition({x, st.y});

        auto& f = m_fields;
        if (f->startPos) f->startPos->release();
        f->startPos = sp;
        f->placed = true;
        this->setStartPosObject(sp);
        m_isTestMode = true;   // a start pos run, so the game itself saves no progress from it
        this->resetLevel();
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        auto& d = ludo::Drill::get();
        int id = m_level->m_levelID.value();
        if (!d.drilling(id)) return;
        auto* spot = d.current();
        if (!spot) return;
        m_fields->drilling = true;
        this->placeStart(*spot);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto& f = m_fields;
        if (!f->drilling || !f->placed || m_isPaused) return;
        if (!m_player1 || m_player1->m_isDead) return;
        auto& d = ludo::Drill::get();
        auto* spot = d.current();
        if (!spot) return;
        if (this->getCurrentPercent() < spot->endPercent()) return;

        // a clean run of the spot
        bool advanced = d.onPass(*spot);
        if (!advanced) {
            Notification::create(fmt::format("Clean {}/{}", spot->clean, ludo::Drill::kCleanToAdvance), NotificationIcon::Success, 0.8f)->show();
            this->resetLevel();
            return;
        }
        Notification::create("Spot cleared", NotificationIcon::Success, 1.5f)->show();
        if (auto* next = d.nextDue(spot->levelId); next && next->levelId == spot->levelId) {
            auto& sess = d.session();
            sess.from = next->from;
            sess.to = next->to;
            this->placeStart(*next);
            return;
        }
        // nothing else due on this level: the session ends quietly
        int left = d.dueCount();
        d.session().active = false;
        f->drilling = false;
        Notification::create(left > 0 ? fmt::format("Level done - {} more due elsewhere", left) : std::string("Drill done for today"),
            NotificationIcon::Success, 2.5f)->show();
        this->onQuit();
    }

    void onExit() {
        if (m_fields->drilling) {
            ludo::Drill::get().session().active = false;
            m_fields->drilling = false;
        }
        PlayLayer::onExit();
    }
};
