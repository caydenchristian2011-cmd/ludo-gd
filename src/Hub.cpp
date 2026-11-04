#include "Hub.hpp"
#include "Store.hpp"
#include <algorithm>

using namespace geode::prelude;

namespace ludo {

static std::string shorten(std::string const& s, size_t n) {
    if (s.size() <= n) return s;
    return s.substr(0, n - 2) + "..";
}

static CCLabelBMFont* makeLabel(CCNode* parent, std::string const& text, char const* font, float scale, CCPoint pos, CCPoint anchor = {0.5f, 0.5f}) {
    auto l = CCLabelBMFont::create(text.c_str(), font);
    l->setScale(scale);
    l->setAnchorPoint(anchor);
    l->setPosition(pos);
    parent->addChild(l);
    return l;
}

static void openLevelPage(int id) {
    auto obj = GJSearchObject::create(SearchType::Search, std::to_string(id));
    auto scene = LevelBrowserLayer::scene(obj);
    CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, scene));
}

// ---------------------------------------------------------------- Hub

Hub* Hub::create() {
    auto ret = new Hub();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool Hub::init() {
    if (!Popup::init(460.f, 290.f, "GJ_square02.png")) return false;
    this->setTitle("Ludo");
    m_root = CCNode::create();
    m_root->setContentSize(m_mainLayer->getContentSize());
    m_root->setPosition({0, 0});
    m_mainLayer->addChild(m_root);
    this->build();
    return true;
}

void Hub::build() {
    m_root->removeAllChildren();
    auto size = m_mainLayer->getContentSize();

    // tab bar
    auto bar = CCMenu::create();
    bar->setPosition({0, 0});
    m_root->addChild(bar);
    struct T { char const* name; Tab tab; };
    T tabs[] = {{"Overview", Tab::Overview}, {"Levels", Tab::Levels}, {"Collections", Tab::Collections}};
    float x = size.width / 2 - 125.f;
    for (auto& t : tabs) {
        bool active = t.tab == m_tab;
        auto spr = ButtonSprite::create(t.name, 110, true, "goldFont.fnt", active ? "GJ_button_02.png" : "GJ_button_01.png", 28.f, 0.6f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(Hub::onTab));
        btn->setTag(static_cast<int>(t.tab));
        btn->setPosition({x, size.height - 52.f});
        bar->addChild(btn);
        x += 125.f;
    }

    auto area = CCSize{size.width - 40.f, size.height - 100.f};
    auto content = CCNode::create();
    content->setPosition({20.f, 20.f});
    content->setContentSize(area);
    m_root->addChild(content);

    switch (m_tab) {
        case Tab::Overview: this->buildOverview(content, area); break;
        case Tab::Levels: this->buildLevels(content, area); break;
        case Tab::Collections:
            if (m_viewing >= 0) this->buildCollectionDetail(content, area);
            else this->buildCollections(content, area);
            break;
    }
}

void Hub::onTab(CCObject* sender) {
    m_tab = static_cast<Tab>(static_cast<CCNode*>(sender)->getTag());
    m_viewing = -1;
    this->build();
}

// ---------------------------------------------------------------- Overview

void Hub::buildOverview(CCNode* root, CCSize area) {
    auto& st = Store::get();
    if (st.levelCount() == 0) {
        makeLabel(root, "Nothing tracked yet", "goldFont.fnt", 0.8f, {area.width / 2, area.height / 2 + 14});
        makeLabel(root, "Play an online level and your stats show up here.", "chatFont.fnt", 0.8f, {area.width / 2, area.height / 2 - 12});
        return;
    }

    struct Tile { std::string value; char const* caption; };
    Tile tiles[] = {
        {formatDuration(st.totalSeconds()), "played"},
        {std::to_string(st.totalAttempts()), "attempts"},
        {std::to_string(st.totalDeaths()), "deaths"},
        {std::to_string(st.totalCompletions()), "completions"},
        {std::to_string(st.levelCount()), "levels"},
    };
    float tileW = area.width / 5.f;
    for (int i = 0; i < 5; i++) {
        float cx = tileW * (static_cast<float>(i) + 0.5f);
        auto bg = CCScale9Sprite::create("square02_001.png", {0, 0, 20, 20});
        bg->setContentSize({tileW - 8.f, 52.f});
        bg->setOpacity(90);
        bg->setPosition({cx, area.height - 30.f});
        root->addChild(bg);
        makeLabel(root, tiles[i].value, "bigFont.fnt", 0.5f, {cx, area.height - 22.f});
        makeLabel(root, tiles[i].caption, "chatFont.fnt", 0.7f, {cx, area.height - 44.f});
    }

    makeLabel(root, "Most played", "goldFont.fnt", 0.6f, {4.f, area.height - 74.f}, {0.f, 0.5f});
    auto top = st.byTime();
    float y = area.height - 98.f;
    int shown = 0;
    for (auto* l : top) {
        if (l->seconds < 1.f && shown > 0) break;
        if (shown >= 4) break;
        std::string name = l->name.empty() ? fmt::format("Level {}", l->id) : l->name;
        makeLabel(root, fmt::format("{}.  {}", shown + 1, shorten(name, 28)), "bigFont.fnt", 0.4f, {4.f, y}, {0.f, 0.5f});
        makeLabel(root, fmt::format("{}   {} deaths", formatDuration(l->seconds), l->deaths), "chatFont.fnt", 0.8f, {area.width - 4.f, y}, {1.f, 0.5f});
        y -= 24.f;
        shown++;
    }
}

// ---------------------------------------------------------------- Levels

void Hub::onFavFilter(CCObject*) {
    m_favOnly = !m_favOnly;
    this->build();
}

void Hub::onFav(CCObject* sender) {
    int id = static_cast<CCNode*>(sender)->getTag();
    if (auto* l = Store::get().find(id)) {
        Store::get().toggleFavorite(id, l->name, l->creator);
    }
    this->build();
}

void Hub::onOpenLevel(CCObject* sender) {
    openLevelPage(static_cast<CCNode*>(sender)->getTag());
}

void Hub::buildLevels(CCNode* root, CCSize area) {
    auto& st = Store::get();

    // filter row
    auto fm = CCMenu::create();
    fm->setPosition({0, 0});
    root->addChild(fm);
    auto toggler = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(Hub::onFavFilter), 0.6f);
    toggler->toggle(m_favOnly);
    toggler->setPosition({14.f, area.height - 12.f});
    fm->addChild(toggler);
    makeLabel(root, "Favorites only", "chatFont.fnt", 0.8f, {32.f, area.height - 12.f}, {0.f, 0.5f});

    auto list = st.byRecent();
    if (m_favOnly) {
        list.erase(std::remove_if(list.begin(), list.end(), [](LevelStat* l) { return !l->favorite; }), list.end());
    }
    if (list.empty()) {
        makeLabel(root, m_favOnly ? "No favorites yet. Star a level from its page." : "No levels yet.", "chatFont.fnt", 0.9f, {area.width / 2, area.height / 2});
        return;
    }

    float listH = area.height - 30.f;
    auto scroll = ScrollLayer::create({area.width, listH});
    scroll->setPosition({0, 0});
    scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(4.f));
    root->addChild(scroll);

    for (auto* l : list) {
        auto row = CCNode::create();
        row->setContentSize({area.width, 34.f});

        auto bg = CCScale9Sprite::create("square02_001.png", {0, 0, 20, 20});
        bg->setContentSize({area.width, 34.f});
        bg->setOpacity(70);
        bg->setAnchorPoint({0, 0});
        row->addChild(bg);

        std::string name = l->name.empty() ? fmt::format("Level {}", l->id) : l->name;
        makeLabel(row, shorten(name, 30), "bigFont.fnt", 0.42f, {10.f, 23.f}, {0.f, 0.5f});
        makeLabel(row, fmt::format("{}   {} attempts   {} deaths   best {}%", formatDuration(l->seconds), l->attempts, l->deaths, l->best),
            "chatFont.fnt", 0.7f, {10.f, 9.f}, {0.f, 0.5f});

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        row->addChild(menu);

        auto star = CircleButtonSprite::createWithSpriteFrameName("GJ_starsIcon_001.png", 0.9f,
            l->favorite ? CircleBaseColor::Green : CircleBaseColor::Gray, CircleBaseSize::Tiny);
        auto favBtn = CCMenuItemSpriteExtra::create(star, this, menu_selector(Hub::onFav));
        favBtn->setTag(l->id);
        favBtn->setPosition({area.width - 54.f, 17.f});
        menu->addChild(favBtn);

        auto go = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
        go->setFlipX(true);
        go->setScale(0.45f);
        auto goBtn = CCMenuItemSpriteExtra::create(go, this, menu_selector(Hub::onOpenLevel));
        goBtn->setTag(l->id);
        goBtn->setPosition({area.width - 20.f, 17.f});
        menu->addChild(goBtn);

        scroll->m_contentLayer->addChild(row);
    }
    scroll->m_contentLayer->updateLayout();
    scroll->scrollToTop();
}

// ---------------------------------------------------------------- Collections

void Hub::onNewCollection(CCObject*) {
    // small popup with a text box; creating happens on "Create"
    struct NamePopup : Popup {
        Hub* hub = nullptr;
        TextInput* input = nullptr;
        static NamePopup* make(Hub* h) {
            auto r = new NamePopup();
            if (r && r->setup(h)) { r->autorelease(); return r; }
            CC_SAFE_DELETE(r);
            return nullptr;
        }
        bool setup(Hub* h) {
            if (!Popup::init(280.f, 140.f, "GJ_square02.png")) return false;
            hub = h;
            this->setTitle("New collection");
            auto sz = m_mainLayer->getContentSize();
            input = TextInput::create(220.f, "Name", "bigFont.fnt");
            input->setMaxCharCount(30);
            input->setPosition({sz.width / 2, sz.height / 2 + 2.f});
            m_mainLayer->addChild(input);
            auto menu = CCMenu::create();
            menu->setPosition({0, 0});
            m_mainLayer->addChild(menu);
            auto btn = CCMenuItemSpriteExtra::create(ButtonSprite::create("Create", 90, true, "goldFont.fnt", "GJ_button_01.png", 28.f, 0.7f), this, menu_selector(NamePopup::onCreate));
            btn->setPosition({sz.width / 2, 28.f});
            menu->addChild(btn);
            return true;
        }
        void onCreate(CCObject*) {
            std::string name = input->getString();
            while (!name.empty() && name.front() == ' ') name.erase(name.begin());
            while (!name.empty() && name.back() == ' ') name.pop_back();
            if (name.empty()) return;
            Store::get().createCollection(name);
            hub->build();
            this->onClose(nullptr);
        }
    };
    if (auto p = NamePopup::make(this)) p->show();
}

void Hub::onViewCollection(CCObject* sender) {
    m_viewing = static_cast<CCNode*>(sender)->getTag();
    this->build();
}

void Hub::onBack(CCObject*) {
    m_viewing = -1;
    this->build();
}

void Hub::onDeleteCollection(CCObject* sender) {
    size_t idx = static_cast<size_t>(static_cast<CCNode*>(sender)->getTag());
    auto& cols = Store::get().collections();
    if (idx >= cols.size()) return;
    createQuickPopup("Delete collection", fmt::format("Delete <cy>{}</c>? Your levels and stats are kept.", cols[idx].name), "Cancel", "Delete",
        [this, idx](FLAlertLayer*, bool yes) {
            if (!yes) return;
            Store::get().deleteCollection(idx);
            this->build();
        });
}

void Hub::onRemoveFromCollection(CCObject* sender) {
    if (m_viewing < 0) return;
    Store::get().removeFromCollection(static_cast<size_t>(m_viewing), static_cast<CCNode*>(sender)->getTag());
    this->build();
}

void Hub::buildCollections(CCNode* root, CCSize area) {
    auto& cols = Store::get().collections();

    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    root->addChild(menu);
    auto add = CCMenuItemSpriteExtra::create(ButtonSprite::create("New collection", 130, true, "goldFont.fnt", "GJ_button_01.png", 26.f, 0.55f), this, menu_selector(Hub::onNewCollection));
    add->setPosition({area.width - 70.f, area.height - 12.f});
    menu->addChild(add);

    if (cols.empty()) {
        makeLabel(root, "No collections yet", "goldFont.fnt", 0.7f, {area.width / 2, area.height / 2 + 10.f});
        makeLabel(root, "Make one here, then add levels from a level's page.", "chatFont.fnt", 0.8f, {area.width / 2, area.height / 2 - 12.f});
        return;
    }

    float listH = area.height - 30.f;
    auto scroll = ScrollLayer::create({area.width, listH});
    scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(4.f));
    root->addChild(scroll);

    for (size_t i = 0; i < cols.size(); i++) {
        auto row = CCNode::create();
        row->setContentSize({area.width, 34.f});
        auto bg = CCScale9Sprite::create("square02_001.png", {0, 0, 20, 20});
        bg->setContentSize({area.width, 34.f});
        bg->setOpacity(70);
        bg->setAnchorPoint({0, 0});
        row->addChild(bg);

        double secs = 0;
        for (int id : cols[i].ids) if (auto* l = Store::get().find(id)) secs += l->seconds;
        makeLabel(row, shorten(cols[i].name, 28), "bigFont.fnt", 0.45f, {10.f, 23.f}, {0.f, 0.5f});
        makeLabel(row, fmt::format("{} levels   {} played", cols[i].ids.size(), formatDuration(secs)), "chatFont.fnt", 0.7f, {10.f, 9.f}, {0.f, 0.5f});

        auto m = CCMenu::create();
        m->setPosition({0, 0});
        row->addChild(m);
        auto view = CCMenuItemSpriteExtra::create(ButtonSprite::create("Open", 60, true, "goldFont.fnt", "GJ_button_01.png", 24.f, 0.5f), this, menu_selector(Hub::onViewCollection));
        view->setTag(static_cast<int>(i));
        view->setPosition({area.width - 80.f, 17.f});
        m->addChild(view);
        auto del = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
        del->setScale(0.6f);
        auto delBtn = CCMenuItemSpriteExtra::create(del, this, menu_selector(Hub::onDeleteCollection));
        delBtn->setTag(static_cast<int>(i));
        delBtn->setPosition({area.width - 22.f, 17.f});
        m->addChild(delBtn);

        scroll->m_contentLayer->addChild(row);
    }
    scroll->m_contentLayer->updateLayout();
    scroll->scrollToTop();
}

void Hub::buildCollectionDetail(CCNode* root, CCSize area) {
    auto& cols = Store::get().collections();
    if (m_viewing < 0 || static_cast<size_t>(m_viewing) >= cols.size()) {
        m_viewing = -1;
        this->buildCollections(root, area);
        return;
    }
    auto& col = cols[static_cast<size_t>(m_viewing)];

    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    root->addChild(menu);
    auto back = CCMenuItemSpriteExtra::create(ButtonSprite::create("Back", 60, true, "goldFont.fnt", "GJ_button_01.png", 24.f, 0.5f), this, menu_selector(Hub::onBack));
    back->setPosition({34.f, area.height - 12.f});
    menu->addChild(back);
    makeLabel(root, shorten(col.name, 30), "goldFont.fnt", 0.7f, {area.width / 2 + 20.f, area.height - 12.f});

    if (col.ids.empty()) {
        makeLabel(root, "Empty. Add levels from a level's page (folder button).", "chatFont.fnt", 0.8f, {area.width / 2, area.height / 2});
        return;
    }

    float listH = area.height - 30.f;
    auto scroll = ScrollLayer::create({area.width, listH});
    scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(4.f));
    root->addChild(scroll);

    for (int id : col.ids) {
        auto* l = Store::get().find(id);
        auto row = CCNode::create();
        row->setContentSize({area.width, 34.f});
        auto bg = CCScale9Sprite::create("square02_001.png", {0, 0, 20, 20});
        bg->setContentSize({area.width, 34.f});
        bg->setOpacity(70);
        bg->setAnchorPoint({0, 0});
        row->addChild(bg);

        std::string name = (l && !l->name.empty()) ? l->name : fmt::format("Level {}", id);
        makeLabel(row, shorten(name, 30), "bigFont.fnt", 0.42f, {10.f, 23.f}, {0.f, 0.5f});
        if (l) {
            makeLabel(row, fmt::format("{}   {} attempts   {} deaths   best {}%", formatDuration(l->seconds), l->attempts, l->deaths, l->best),
                "chatFont.fnt", 0.7f, {10.f, 9.f}, {0.f, 0.5f});
        }
        auto m = CCMenu::create();
        m->setPosition({0, 0});
        row->addChild(m);
        auto go = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
        go->setFlipX(true);
        go->setScale(0.45f);
        auto goBtn = CCMenuItemSpriteExtra::create(go, this, menu_selector(Hub::onOpenLevel));
        goBtn->setTag(id);
        goBtn->setPosition({area.width - 54.f, 17.f});
        m->addChild(goBtn);
        auto x = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
        x->setScale(0.55f);
        auto xBtn = CCMenuItemSpriteExtra::create(x, this, menu_selector(Hub::onRemoveFromCollection));
        xBtn->setTag(id);
        xBtn->setPosition({area.width - 20.f, 17.f});
        m->addChild(xBtn);

        scroll->m_contentLayer->addChild(row);
    }
    scroll->m_contentLayer->updateLayout();
    scroll->scrollToTop();
}

// ---------------------------------------------------------------- Picker

PickerPopup* PickerPopup::create(int levelID, std::string name, std::string creator) {
    auto ret = new PickerPopup();
    if (ret && ret->init(levelID, std::move(name), std::move(creator))) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool PickerPopup::init(int levelID, std::string name, std::string creator) {
    if (!Popup::init(300.f, 240.f, "GJ_square02.png")) return false;
    m_levelID = levelID;
    this->setTitle("Add to collection");
    // make sure the level exists in the store so the collection can show its name
    Store::get().level(levelID, name, creator);
    Store::get().saveIfDirty();

    auto size = m_mainLayer->getContentSize();
    auto& cols = Store::get().collections();
    if (cols.empty()) {
        makeLabel(m_mainLayer, "No collections yet.", "goldFont.fnt", 0.7f, {size.width / 2, size.height / 2 + 10.f});
        makeLabel(m_mainLayer, "Create one in the Ludo hub", "chatFont.fnt", 0.8f, {size.width / 2, size.height / 2 - 12.f});
        makeLabel(m_mainLayer, "(button on the main menu).", "chatFont.fnt", 0.8f, {size.width / 2, size.height / 2 - 28.f});
        return true;
    }

    auto scroll = ScrollLayer::create({size.width - 40.f, size.height - 80.f});
    scroll->setPosition({20.f, 20.f});
    scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(4.f));
    m_mainLayer->addChild(scroll);
    for (size_t i = 0; i < cols.size(); i++) {
        auto row = CCNode::create();
        row->setContentSize({size.width - 40.f, 30.f});
        makeLabel(row, shorten(cols[i].name, 24), "bigFont.fnt", 0.42f, {36.f, 15.f}, {0.f, 0.5f});
        auto m = CCMenu::create();
        m->setPosition({0, 0});
        row->addChild(m);
        auto t = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(PickerPopup::onToggle), 0.7f);
        t->toggle(Store::get().inCollection(i, levelID));
        t->setTag(static_cast<int>(i));
        t->setPosition({16.f, 15.f});
        m->addChild(t);
        scroll->m_contentLayer->addChild(row);
    }
    scroll->m_contentLayer->updateLayout();
    scroll->scrollToTop();
    return true;
}

void PickerPopup::onToggle(CCObject* sender) {
    auto t = static_cast<CCMenuItemToggler*>(sender);
    size_t idx = static_cast<size_t>(t->getTag());
    bool nowOn = !t->isToggled();   // the toggler flips after this callback runs
    if (nowOn) Store::get().addToCollection(idx, m_levelID);
    else Store::get().removeFromCollection(idx, m_levelID);
}

}
