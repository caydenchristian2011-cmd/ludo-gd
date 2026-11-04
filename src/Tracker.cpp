// Watches what happens inside a level and writes it into the Store.
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <ctime>
#include "Store.hpp"

using namespace geode::prelude;

class $modify(LudoPlayLayer, PlayLayer) {
    struct Fields {
        int levelID = 0;
        double pending = 0;   // seconds not yet added to the Store
        double sinceSave = 0;
    };

    void flushTime() {
        auto& f = m_fields;
        if (!f->levelID) return;
        auto* s = ludo::Store::get().find(f->levelID);
        if (!s) return;
        s->seconds += f->pending;
        f->pending = 0;
        s->best = std::max(s->best, static_cast<int>(m_level->m_normalPercent.value()));
        ludo::Store::get().dirty();
        ludo::Store::get().saveIfDirty();
        f->sinceSave = 0;
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        int id = level->m_levelID.value();
        if (id > 0) {   // local and editor levels have no online id; Ludo skips them
            m_fields->levelID = id;
            auto& s = ludo::Store::get().level(id, static_cast<std::string>(level->m_levelName), static_cast<std::string>(level->m_creatorName));
            s.last = static_cast<long long>(std::time(nullptr));
            ludo::Store::get().dirty();
        }
        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto& f = m_fields;
        if (!f->levelID || m_isPaused) return;
        double step = std::min<double>(dt, 0.5);   // a long hitch is not play time
        f->pending += step;
        f->sinceSave += step;
        if (f->sinceSave >= 30.0) flushTime();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        if (!m_fields->levelID) return;
        if (auto* s = ludo::Store::get().find(m_fields->levelID)) {
            s->attempts++;
            ludo::Store::get().dirty();
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        bool wasDead = (m_player1 && m_player1->m_isDead) || (m_player2 && m_player2->m_isDead);
        bool isAnticheat = object && object == m_anticheatSpike;
        PlayLayer::destroyPlayer(player, object);
        if (isAnticheat || wasDead || !m_fields->levelID) return;
        bool nowDead = (m_player1 && m_player1->m_isDead) || (m_player2 && m_player2->m_isDead);
        if (!nowDead) return;
        if (auto* s = ludo::Store::get().find(m_fields->levelID)) {
            s->deaths++;
            ludo::Store::get().dirty();
        }
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        if (!m_fields->levelID) return;
        if (auto* s = ludo::Store::get().find(m_fields->levelID)) {
            s->completions++;
            s->best = 100;
            ludo::Store::get().dirty();
        }
        flushTime();
    }

    void onExit() {
        flushTime();
        PlayLayer::onExit();
    }
};
