#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include "Store.hpp"
#include "Hub.hpp"

using namespace geode::prelude;

class $modify(LudoMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto sprite = CCSprite::createWithSpriteFrameName("GJ_statsBtn_001.png");
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(LudoMenuLayer::onLudo));
        button->setID("ludo-button"_spr);
        if (auto menu = this->getChildByID("bottom-menu")) {
            menu->addChild(button);
            menu->updateLayout();
        }
        return true;
    }

    void onLudo(CCObject*) {
        if (auto hub = ludo::Hub::create()) hub->show();
    }
};

// Star and collection buttons on every online level's page.
class $modify(LudoInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;
        int id = level->m_levelID.value();
        if (id <= 0) return true;
        auto menu = this->getChildByID("left-side-menu");
        if (!menu) return true;

        bool fav = ludo::Store::get().isFavorite(id);
        auto on = CircleButtonSprite::createWithSpriteFrameName("GJ_starsIcon_001.png", 0.9f, CircleBaseColor::Green, CircleBaseSize::Small);
        auto off = CircleButtonSprite::createWithSpriteFrameName("GJ_starsIcon_001.png", 0.9f, CircleBaseColor::Gray, CircleBaseSize::Small);
        auto star = CCMenuItemToggler::create(off, on, this, menu_selector(LudoInfoLayer::onLudoFav));
        star->toggle(fav);
        star->setID("ludo-favorite"_spr);
        menu->addChild(star);

        auto folder = CircleButtonSprite::createWithSpriteFrameName("folderIcon_001.png", 0.9f, CircleBaseColor::Blue, CircleBaseSize::Small);
        auto folderBtn = CCMenuItemSpriteExtra::create(folder, this, menu_selector(LudoInfoLayer::onLudoCollect));
        folderBtn->setID("ludo-collections"_spr);
        menu->addChild(folderBtn);

        menu->updateLayout();
        return true;
    }

    void onLudoFav(CCObject* sender) {
        auto t = static_cast<CCMenuItemToggler*>(sender);
        (void)t;
        ludo::Store::get().toggleFavorite(
            m_level->m_levelID.value(), static_cast<std::string>(m_level->m_levelName), static_cast<std::string>(m_level->m_creatorName));
    }

    void onLudoCollect(CCObject*) {
        if (auto p = ludo::PickerPopup::create(
                m_level->m_levelID.value(), static_cast<std::string>(m_level->m_levelName), static_cast<std::string>(m_level->m_creatorName))) {
            p->show();
        }
    }
};
