#pragma once
#include <Geode/Geode.hpp>

namespace ludo {

// The Ludo hub: Overview, Levels and Collections.
class Hub : public geode::Popup {
public:
    static Hub* create();

protected:
    bool init();

    enum class Tab { Overview, Levels, Collections };

    void build();                 // rebuilds the tab bar and the current tab
    void buildOverview(cocos2d::CCNode* root, cocos2d::CCSize area);
    void buildLevels(cocos2d::CCNode* root, cocos2d::CCSize area);
    void buildCollections(cocos2d::CCNode* root, cocos2d::CCSize area);
    void buildCollectionDetail(cocos2d::CCNode* root, cocos2d::CCSize area);

    void onTab(cocos2d::CCObject*);
    void onFavFilter(cocos2d::CCObject*);
    void onFav(cocos2d::CCObject*);
    void onOpenLevel(cocos2d::CCObject*);
    void onNewCollection(cocos2d::CCObject*);
    void onViewCollection(cocos2d::CCObject*);
    void onDeleteCollection(cocos2d::CCObject*);
    void onRemoveFromCollection(cocos2d::CCObject*);
    void onBack(cocos2d::CCObject*);

    cocos2d::CCNode* m_root = nullptr;
    Tab m_tab = Tab::Overview;
    bool m_favOnly = false;
    int m_viewing = -1;           // collection being viewed, or -1
};

// "Add to a collection" picker used from the level page.
class PickerPopup : public geode::Popup {
public:
    static PickerPopup* create(int levelID, std::string name, std::string creator);

protected:
    bool init(int levelID, std::string name, std::string creator);
    void onToggle(cocos2d::CCObject*);

    int m_levelID = 0;
};

}
