#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "Store.hpp"

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
        auto& st = ludo::Store::get();
        auto body = fmt::format(
            "<cy>{}</c> levels tracked\n<cg>{}</c> played\n{} attempts, {} deaths, {} completions",
            st.levelCount(), ludo::formatDuration(st.totalSeconds()), st.totalAttempts(), st.totalDeaths(), st.totalCompletions());
        FLAlertLayer::create("Ludo", body, "OK")->show();
    }
};
