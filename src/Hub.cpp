#include "Hub.hpp"
#include "Store.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>

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

// A dark rounded card used for tiles and rows.
static CCScale9Sprite* makeCard(CCSize size, GLubyte opacity = 255) {
    auto bg = CCScale9Sprite::create("ludo_tile.png"_spr, {0, 0, 48, 48});
    bg->setContentSize(size);
    bg->setOpacity(opacity);
    return bg;
}

// A number that counts up from zero when it appears.
class CountUp : public CCNode {
public:
    static CountUp* create(double target, bool isTime, float scale, char const* font) {
        auto ret = new CountUp();
        if (ret && ret->setup(target, isTime, scale, font)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool setup(double target, bool isTime, float scale, char const* font) {
        if (!CCNode::init()) return false;
        m_target = target;
        m_isTime = isTime;
        m_label = CCLabelBMFont::create("0", font);
        m_label->setScale(scale);
        this->addChild(m_label);
        this->show(0.0);
        this->scheduleUpdate();
        return true;
    }

    void update(float dt) override {
        m_t += dt;
        float p = std::min(1.f, m_t / 0.9f);
        float e = 1.f - (1.f - p) * (1.f - p) * (1.f - p);
        this->show(m_target * e);
        if (p >= 1.f) this->unscheduleUpdate();
    }

private:
    void show(double v) {
        std::string text = m_isTime ? formatDuration(v) : std::to_string(static_cast<long long>(v + 0.5));
        m_label->setString(text.c_str());
    }

    CCLabelBMFont* m_label = nullptr;
    double m_target = 0;
    bool m_isTime = false;
    float m_t = 0;
};

// Ink mountain: one peak for every slice of the level where you die.
static void drawMountain(CCNode* parent, std::array<int, 20> const& hist, CCSize size) {
    int peak = 0;
    for (int h : hist) peak = std::max(peak, h);
    if (peak <= 0) return;

    auto draw = CCDrawNode::create();
    parent->addChild(draw);

    constexpr int N = 80;   // sample points across the width
    std::vector<float> hs(N + 1);
    for (int i = 0; i <= N; i++) {
        float pos = std::clamp(static_cast<float>(i) / N * 20.f - 0.5f, 0.f, 19.f);
        int a = static_cast<int>(pos);
        int b = std::min(a + 1, 19);
        float f = pos - static_cast<float>(a);
        f = f * f * (3.f - 2.f * f);   // smoothstep
        float v = static_cast<float>(hist[a]) * (1.f - f) + static_cast<float>(hist[b]) * f;
        hs[i] = v / static_cast<float>(peak) * size.height;
    }
    ccColor4F fill = {0.93f, 0.89f, 0.87f, 0.16f};
    ccColor4F line = {1.f, 0.28f, 0.34f, 1.f};
    for (int i = 0; i < N; i++) {
        float x0 = size.width * static_cast<float>(i) / N;
        float x1 = size.width * static_cast<float>(i + 1) / N;
        CCPoint quad[4] = {{x0, 0}, {x0, hs[i]}, {x1, hs[i + 1]}, {x1, 0}};
        draw->drawPolygon(quad, 4, fill, 0.f, fill);
        draw->drawSegment({x0, hs[i]}, {x1, hs[i + 1]}, 0.8f, line);
    }
    draw->drawSegment({0, 0}, {size.width, 0}, 0.6f, {0.93f, 0.89f, 0.87f, 0.35f});
}

// Sun (or moon), drifting petals, all behind the panel's content.
static void addBackdrop(CCNode* layer, CCSize size, CCNode* bg) {
    layer->reorderChild(bg, -10);

    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    int hour = local.tm_hour;
    auto sun = CCSprite::create("ludo_sun.png"_spr);
    if (sun) {
        sun->setScale(0.62f);
        sun->setPosition({size.width - 78.f, size.height - 74.f});
        sun->setOpacity(125);
        if (hour >= 22 || hour < 5) {            // night: a pale moon
            sun->setColor({205, 214, 255});
            sun->setOpacity(95);
        } else if (hour < 8 || hour >= 17) {      // dawn and dusk: a warm sun
            sun->setColor({255, 150, 90});
        }
        sun->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCScaleTo::create(4.f, 0.65f)),
            CCEaseSineInOut::create(CCScaleTo::create(4.f, 0.60f)),
            nullptr)));
        layer->addChild(sun, -5);
    }

    for (int i = 0; i < 6; i++) {
        auto petal = CCSprite::create("ludo_petal.png"_spr);
        if (!petal) break;
        float fall = 2.6f * static_cast<float>(i % 3) + 9.f;
        float startX = 40.f + (size.width - 80.f) * static_cast<float>(i) / 5.f;
        float startY = size.height - 22.f;
        petal->setScale(0.42f + 0.06f * static_cast<float>(i % 3));
        petal->setOpacity(0);
        petal->setPosition({startX, startY});
        petal->runAction(CCSequence::create(
            CCDelayTime::create(1.4f * static_cast<float>(i)),
            CCRepeatForever::create(CCSequence::create(
                CCPlace::create({startX, startY}),
                CCSpawn::create(
                    CCMoveBy::create(fall, {(i % 2 ? 36.f : -36.f), -(size.height - 48.f)}),
                    CCRotateBy::create(fall, (i % 2 ? 300.f : -300.f)),
                    CCSequence::create(CCFadeTo::create(1.2f, 170), CCDelayTime::create(fall - 2.8f), CCFadeTo::create(1.6f, 0), nullptr),
                    nullptr),
                nullptr)),
            nullptr));
        layer->addChild(petal, -4);
    }
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
    if (!Popup::init(460.f, 290.f, "ludo_panel.png"_spr)) return false;
    this->setTitle("Ludo");
    addBackdrop(m_mainLayer, m_mainLayer->getContentSize(), m_bgSprite);
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

    struct Tile { double value; bool isTime; char const* caption; };
    Tile tiles[] = {
        {st.totalSeconds(), true, "played"},
        {static_cast<double>(st.totalAttempts()), false, "attempts"},
        {static_cast<double>(st.totalDeaths()), false, "deaths"},
        {static_cast<double>(st.totalCompletions()), false, "completions"},
        {static_cast<double>(st.levelCount()), false, "levels"},
    };
    float tileW = area.width / 5.f;
    for (int i = 0; i < 5; i++) {
        float cx = tileW * (static_cast<float>(i) + 0.5f);
        auto bg = makeCard({tileW - 8.f, 52.f}, 210);
        bg->setPosition({cx, area.height - 30.f});
        root->addChild(bg);
        auto num = CountUp::create(tiles[i].value, tiles[i].isTime, 0.5f, "bigFont.fnt");
        num->setPosition({cx, area.height - 22.f});
        root->addChild(num);
        makeLabel(root, tiles[i].caption, "chatFont.fnt", 0.7f, {cx, area.height - 44.f});
    }

    // left: most played
    float colTop = area.height - 74.f;
    makeLabel(root, "Most played", "goldFont.fnt", 0.6f, {4.f, colTop}, {0.f, 0.5f});
    auto top = st.byTime();
    float y = colTop - 24.f;
    int shown = 0;
    for (auto* l : top) {
        if (l->seconds < 1.f && shown > 0) break;
        if (shown >= 4) break;
        std::string name = l->name.empty() ? fmt::format("Level {}", l->id) : l->name;
        makeLabel(root, fmt::format("{}.  {}", shown + 1, shorten(name, 17)), "bigFont.fnt", 0.36f, {4.f, y}, {0.f, 0.5f});
        makeLabel(root, formatDuration(l->seconds), "chatFont.fnt", 0.75f, {196.f, y}, {1.f, 0.5f});
        y -= 22.f;
        shown++;
    }

    // right: the death mountain of the level you die in most
    LevelStat* worst = nullptr;
    int worstSum = 0;
    for (auto* l : top) {
        int sum = 0;
        for (int h : l->hist) sum += h;
        if (sum > worstSum) { worstSum = sum; worst = l; }
    }
    float mx = 220.f, mw = area.width - mx - 4.f;
    makeLabel(root, "Death mountain", "goldFont.fnt", 0.6f, {mx, colTop}, {0.f, 0.5f});
    if (!worst) {
        makeLabel(root, "Play a level and the places you die show up here.", "chatFont.fnt", 0.7f, {mx + mw / 2, colTop - 56.f});
        return;
    }
    std::string wn = worst->name.empty() ? fmt::format("Level {}", worst->id) : worst->name;
    makeLabel(root, shorten(wn, 24), "chatFont.fnt", 0.7f, {mx + mw, colTop}, {1.f, 0.5f});

    auto frame = makeCard({mw, 84.f}, 150);
    frame->setAnchorPoint({0, 0});
    frame->setPosition({mx, colTop - 100.f});
    root->addChild(frame);
    auto peakNode = CCNode::create();
    peakNode->setPosition({mx + 8.f, colTop - 92.f});
    root->addChild(peakNode);
    float gw = mw - 16.f;
    drawMountain(peakNode, worst->hist, {gw, 54.f});

    int peakIdx = 0;
    for (int i = 1; i < 20; i++) if (worst->hist[i] > worst->hist[peakIdx]) peakIdx = i;
    makeLabel(root, fmt::format("most deaths at {}-{}%", peakIdx * 5, peakIdx * 5 + 5), "chatFont.fnt", 0.65f, {mx + mw / 2, colTop - 18.f});
    makeLabel(root, "0%", "chatFont.fnt", 0.55f, {mx + 8.f, colTop - 97.f}, {0.f, 0.5f});
    makeLabel(root, "100%", "chatFont.fnt", 0.55f, {mx + mw - 8.f, colTop - 97.f}, {1.f, 0.5f});
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

        auto bg = makeCard({area.width, 34.f}, 200);
        bg->setAnchorPoint({0, 0});
        row->addChild(bg);

        std::string name = l->name.empty() ? fmt::format("Level {}", l->id) : l->name;
        makeLabel(row, shorten(name, 30), "bigFont.fnt", 0.42f, {10.f, 23.f}, {0.f, 0.5f});
        makeLabel(row, fmt::format("{}   {} tries   {} deaths", formatDuration(l->seconds), l->attempts, l->deaths),
            "chatFont.fnt", 0.7f, {10.f, 9.f}, {0.f, 0.5f});

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        row->addChild(menu);

        auto star = CCSprite::create(l->favorite ? "ludo_blossom_on.png"_spr : "ludo_blossom_off.png"_spr);
        star->setScale(0.5f);
        auto favBtn = CCMenuItemSpriteExtra::create(star, this, menu_selector(Hub::onFav));
        favBtn->setTag(l->id);
        favBtn->setPosition({area.width - 54.f, 17.f});
        menu->addChild(favBtn);

        // red seal for levels you have beaten
        if (l->completions > 0) {
            auto stamp = CCSprite::create("ludo_stamp.png"_spr);
            stamp->setScale(0.36f);
            stamp->setRotation(-6.f);
            stamp->setPosition({area.width - 88.f, 17.f});
            row->addChild(stamp);
        }

        // best percent as a brush stroke under the stats line
        float barW = 60.f;
        auto track = makeCard({barW, 5.f}, 120);
        track->setAnchorPoint({0, 0.5f});
        track->setPosition({area.width - 160.f, 8.f});
        row->addChild(track);
        if (l->best > 0) {
            auto fillBar = makeCard({std::max(8.f, barW * static_cast<float>(l->best) / 100.f), 5.f}, 255);
            fillBar->setColor({232, 52, 70});
            fillBar->setAnchorPoint({0, 0.5f});
            fillBar->setPosition({area.width - 160.f, 8.f});
            row->addChild(fillBar);
        }

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
            if (!Popup::init(280.f, 140.f, "ludo_panel.png"_spr)) return false;
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
        auto bg = makeCard({area.width, 34.f}, 200);
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
        auto bg = makeCard({area.width, 34.f}, 200);
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
    if (!Popup::init(300.f, 240.f, "ludo_panel.png"_spr)) return false;
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
