#include "Hub.hpp"
#include "Store.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <ctime>

using namespace geode::prelude;

namespace ludo {

// ---------------------------------------------------------------- small helpers

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

// Headings use the launcher's serif. If the font did not load, fall back to the game's own.
static CCLabelBMFont* heading(CCNode* parent, std::string const& text, float scale, CCPoint pos, CCPoint anchor = {0.5f, 0.5f},
                              ccColor3B color = {236, 228, 222}) {
    auto l = CCLabelBMFont::create(text.c_str(), "Fraunces.fnt"_spr);
    if (!l) {
        l = CCLabelBMFont::create(text.c_str(), "goldFont.fnt");
        scale *= 0.8f;
    }
    l->setScale(scale);
    l->setAnchorPoint(anchor);
    l->setPosition(pos);
    l->setColor(color);
    parent->addChild(l);
    return l;
}

// A sprite from the mod's images. A missing image gives an empty sprite, never a crash.
static CCSprite* spr(char const* name, float scale = 1.f) {
    auto s = CCSprite::create(name);
    if (!s) s = CCSprite::create();
    s->setScale(scale);
    return s;
}

// Same, for names built at run time ("ludo_petal_2.png").
static CCSprite* sprFile(std::string const& file, float scale = 1.f) {
    return spr(Mod::get()->expandSpriteName(file).c_str(), scale);
}

// A stretchable image (rounded cards, pills, scroll rows). Centered on its position.
static CCNodeRGBA* nine(char const* file, CCSize size, NineSlice::Insets insets = {}) {
    CCNodeRGBA* out = NineSlice::create(file, CCRect{}, insets);
    if (!out) out = CCNodeRGBA::create();
    out->setAnchorPoint({0.5f, 0.5f});
    out->setContentSize(size);
    return out;
}

// A dark rounded card used for tiles and rows.
static CCNodeRGBA* card(CCSize size, GLubyte opacity = 255) {
    auto bg = nine("ludo_tile.png"_spr, size);
    bg->setOpacity(opacity);
    return bg;
}

// Rounded button face with a label: red when it is the main action, dark otherwise.
static CCNode* pill(std::string const& text, float w, bool on, float h = 26.f) {
    auto root = CCNode::create();
    root->setContentSize({w, h});
    auto bg = nine(on ? "ludo_pill_on.png"_spr : "ludo_pill_off.png"_spr, {w, h}, NineSlice::Insets{11.f, 22.f, 11.f, 22.f});
    bg->setPosition({w / 2, h / 2});
    root->addChild(bg);
    auto l = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    l->limitLabelWidth(w - 16.f, 0.42f, 0.1f);
    l->setColor(on ? ccColor3B{255, 255, 255} : ccColor3B{236, 228, 222});
    l->setPosition({w / 2, h / 2 + 1.f});
    root->addChild(l);
    return root;
}

static void openLevelPage(int id) {
    auto obj = GJSearchObject::create(SearchType::Search, std::to_string(id));
    auto scene = LevelBrowserLayer::scene(obj);
    CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, scene));
}

static bool ambience() {
    return Mod::get()->getSettingValue<bool>("ambience");
}

// A number that counts up from zero when it appears.
class CountUp : public CCNode {
public:
    static CountUp* create(double target, bool isTime, float scale, bool animate) {
        auto ret = new CountUp();
        if (ret && ret->setup(target, isTime, scale, animate)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool setup(double target, bool isTime, float scale, bool animate) {
        if (!CCNode::init()) return false;
        m_target = target;
        m_isTime = isTime;
        m_label = CCLabelBMFont::create("0", "Fraunces.fnt"_spr);
        if (!m_label) {
            m_label = CCLabelBMFont::create("0", "bigFont.fnt");
            scale *= 0.8f;
        }
        m_label->setScale(scale);
        this->addChild(m_label);
        if (animate) {
            this->show(0.0);
            this->scheduleUpdate();
        } else {
            this->show(target);
        }
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
        draw->drawSegment({x0, hs[i]}, {x1, hs[i + 1]}, 0.7f, line);
    }
    draw->drawSegment({0, 0}, {size.width, 0}, 0.5f, {0.93f, 0.89f, 0.87f, 0.35f});
}

static int hourNow() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return local.tm_hour;
}

static bool isNight() {
    int h = hourNow();
    return h >= 22 || h < 5;
}

// Sun (or moon), the blossom branch and drifting petals, all behind the panel's content.
static void addBackdrop(CCNode* layer, CCSize size, CCNode* bg) {
    layer->reorderChild(bg, -10);
    bool move = ambience();

    int hour = hourNow();
    auto sun = spr("ludo_sun.png"_spr);
    sun->setPosition({92.f, size.height - 86.f});
    sun->setOpacity(120);
    if (isNight()) {                 // night: a pale moon
        sun->setColor({205, 214, 255});
        sun->setOpacity(90);
    } else if (hour < 8 || hour >= 17) {   // dawn and dusk: a warmer sun
        sun->setColor({255, 160, 110});
    }
    if (move) {
        sun->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCScaleTo::create(4.5f, 1.04f)),
            CCEaseSineInOut::create(CCScaleTo::create(4.5f, 0.97f)),
            nullptr)));
    }
    layer->addChild(sun, -6);

    auto branch = spr("ludo_branch.png"_spr);
    branch->setFlipX(true);
    branch->setAnchorPoint({1.f, 1.f});
    branch->setPosition({size.width - 4.f, size.height - 4.f});
    if (move) {
        branch->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCRotateTo::create(5.5f, 1.2f)),
            CCEaseSineInOut::create(CCRotateTo::create(5.5f, -0.9f)),
            nullptr)));
    }
    layer->addChild(branch, -5);

    if (!move) return;
    for (int i = 0; i < 7; i++) {
        auto petal = sprFile(fmt::format("ludo_petal_{}.png", i % 3));
        float fall = 9.f + 2.4f * static_cast<float>(i % 3);
        float startX = 36.f + (size.width - 72.f) * static_cast<float>(i) / 6.f;
        float startY = size.height - 24.f;
        float dir = (i % 2) ? 1.f : -1.f;
        petal->setScale(0.8f + 0.12f * static_cast<float>(i % 3));
        petal->setOpacity(0);
        petal->setPosition({startX, startY});
        petal->runAction(CCSequence::create(
            CCDelayTime::create(1.5f * static_cast<float>(i)),
            CCRepeatForever::create(CCSequence::create(
                CCPlace::create({startX, startY}),
                CCSpawn::create(
                    CCMoveBy::create(fall, {dir * 38.f, -(size.height - 52.f)}),
                    CCRotateBy::create(fall, dir * 320.f),
                    CCSequence::create(CCFadeTo::create(1.3f, 190), CCDelayTime::create(fall - 3.0f), CCFadeTo::create(1.7f, 0), nullptr),
                    nullptr),
                nullptr)),
            nullptr));
        layer->addChild(petal, -4);
    }
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
    if (!Popup::init(460.f, 290.f, "ludo_hub_bg.png"_spr)) return false;
    auto size = m_size;
    this->setCloseButtonSpr(spr("ludo_ic_x.png"_spr));
    addBackdrop(m_mainLayer, size, m_bgSprite);

    heading(m_mainLayer, "Ludo", 0.62f, {size.width / 2, size.height - 20.f});
    auto brush = spr("ludo_brush.png"_spr);
    brush->setPosition({size.width / 2, size.height - 35.f});
    m_mainLayer->addChild(brush, 1);

    m_root = CCNode::create();
    m_root->setContentSize(size);
    m_root->setPosition({0, 0});
    m_mainLayer->addChild(m_root);
    this->build();

    this->addFriend(size);
    return true;
}

void Hub::build() {
    m_root->removeAllChildren();
    auto size = m_size;

    // tab bar
    auto bar = CCMenu::create();
    bar->setPosition({0, 0});
    m_root->addChild(bar);
    struct T { char const* name; Tab tab; };
    T tabs[] = {{"Overview", Tab::Overview}, {"Levels", Tab::Levels}, {"Collections", Tab::Collections}};
    float x = size.width / 2 - 102.f;
    for (auto& t : tabs) {
        bool active = t.tab == m_tab;
        auto btn = CCMenuItemSpriteExtra::create(pill(t.name, 98.f, active), this, menu_selector(Hub::onTab));
        btn->setTag(static_cast<int>(t.tab));
        btn->setPosition({x, size.height - 54.f});
        bar->addChild(btn);
        x += 102.f;
    }

    auto area = CCSize{size.width - 40.f, size.height - 46.f - 76.f};
    auto content = CCNode::create();
    content->setPosition({20.f, 46.f});
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

    // footer, left of the friend
    makeLabel(m_root, fmt::format("Ludo {}", Mod::get()->getVersion().toVString()), "chatFont.fnt", 0.6f, {24.f, 20.f}, {0.f, 0.5f})
        ->setOpacity(110);
}

void Hub::onTab(CCObject* sender) {
    m_tab = static_cast<Tab>(static_cast<CCNode*>(sender)->getTag());
    m_viewing = -1;
    this->react(2, 0.5f);
    this->build();
}

// ---------------------------------------------------------------- Corner friend

void Hub::addFriend(CCSize size) {
    std::string choice = Mod::get()->getSettingValue<std::string>("friend");
    std::transform(choice.begin(), choice.end(), choice.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (choice != "samurai" && choice != "octopus") return;

    static char const* names[4] = {"neutral", "blink", "happy", "oh"};
    float scale = 0.6f;
    auto holder = CCNode::create();
    holder->setContentSize({120.f, 74.f});
    for (int i = 0; i < 4; i++) {
        auto s = sprFile(fmt::format("ludo_{}_{}.png", choice, names[i]));
        s->setPosition({60.f, 37.f});
        s->setVisible(i == 0);
        holder->addChild(s);
        m_poses[i] = s;
    }
    holder->setScale(scale);

    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_mainLayer->addChild(menu, 5);   // not in m_root: the tabs rebuild m_root
    auto btn = CCMenuItemSpriteExtra::create(holder, this, menu_selector(Hub::onFriend));
    btn->setPosition({size.width - 62.f, 4.f + 74.f * scale / 2});
    menu->addChild(btn);

    m_sleepy = isNight();
    m_pose = 0;
    if (!m_sleepy) this->react(2, 1.6f);   // says hello
    this->schedule(schedule_selector(Hub::friendTick), 0.1f);
}

void Hub::showPose(int pose) {
    if (pose == m_pose) return;
    m_pose = pose;
    for (int i = 0; i < 4; i++) {
        if (m_poses[i]) m_poses[i]->setVisible(i == pose);
    }
}

void Hub::react(int pose, float seconds) {
    if (!m_poses[0]) return;
    m_reactPose = pose;
    m_reactUntil = m_clock + seconds;
    this->showPose(pose);
}

void Hub::friendTick(float dt) {
    m_clock += dt;
    if (m_reactUntil > m_clock) {
        this->showPose(m_reactPose);
        return;
    }
    if (m_sleepy) {                 // it is late: the friend is dozing
        this->showPose(1);
        return;
    }
    if (m_blinkUntil > m_clock) {
        this->showPose(1);
    } else if (m_clock >= m_nextBlink) {
        m_blinkUntil = m_clock + 0.16f;
        m_nextBlink = m_clock + 2.5f + static_cast<float>(std::rand() % 3000) / 1000.f;
        this->showPose(1);
    } else {
        this->showPose(0);
    }
}

void Hub::onFriend(CCObject*) {
    if (!m_poses[0]) return;
    bool startled = (std::rand() % 4) == 0;
    m_sleepy = false;   // a poke wakes it up
    this->react(startled ? 3 : 2, 1.4f);
}

// ---------------------------------------------------------------- Overview

void Hub::buildOverview(CCNode* root, CCSize area) {
    auto& st = Store::get();
    if (st.levelCount() == 0) {
        heading(root, "Nothing tracked yet", 0.5f, {area.width / 2, area.height / 2 + 14});
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
    bool animate = ambience();
    float tileW = area.width / 5.f;
    for (int i = 0; i < 5; i++) {
        float cx = tileW * (static_cast<float>(i) + 0.5f);
        auto bg = card({tileW - 8.f, 52.f}, 235);
        bg->setPosition({cx, area.height - 28.f});
        root->addChild(bg);
        auto num = CountUp::create(tiles[i].value, tiles[i].isTime, 0.5f, animate);
        num->setPosition({cx, area.height - 22.f});
        root->addChild(num);
        makeLabel(root, tiles[i].caption, "chatFont.fnt", 0.7f, {cx, area.height - 42.f})->setOpacity(190);
    }

    // left: most played
    float colTop = area.height - 72.f;
    heading(root, "Most played", 0.34f, {4.f, colTop}, {0.f, 0.5f}, {255, 120, 130});
    auto top = st.byTime();
    float y = colTop - 22.f;
    int shown = 0;
    for (auto* l : top) {
        if (l->seconds < 1.f && shown > 0) break;
        if (shown >= 4) break;
        std::string name = l->name.empty() ? fmt::format("Level {}", l->id) : l->name;
        makeLabel(root, fmt::format("{}.  {}", shown + 1, shorten(name, 17)), "bigFont.fnt", 0.34f, {4.f, y}, {0.f, 0.5f});
        makeLabel(root, formatDuration(l->seconds), "chatFont.fnt", 0.75f, {196.f, y}, {1.f, 0.5f})->setOpacity(200);
        y -= 20.f;
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
    heading(root, "Death mountain", 0.34f, {mx, colTop}, {0.f, 0.5f}, {255, 120, 130});
    if (!worst) {
        makeLabel(root, "Play a level and the places you die show up here.", "chatFont.fnt", 0.7f, {mx + mw / 2, colTop - 44.f});
        return;
    }
    std::string wn = worst->name.empty() ? fmt::format("Level {}", worst->id) : worst->name;
    makeLabel(root, shorten(wn, 24), "chatFont.fnt", 0.7f, {mx + mw, colTop}, {1.f, 0.5f})->setOpacity(200);

    auto frame = card({mw, 80.f}, 190);
    frame->setPosition({mx + mw / 2, 2.f + 40.f});
    root->addChild(frame);

    int peakIdx = 0;
    for (int i = 1; i < 20; i++) if (worst->hist[i] > worst->hist[peakIdx]) peakIdx = i;
    makeLabel(root, fmt::format("most deaths at {}-{}%", peakIdx * 5, peakIdx * 5 + 5), "chatFont.fnt", 0.65f, {mx + mw / 2, 70.f});

    auto graph = CCNode::create();
    graph->setPosition({mx + 8.f, 17.f});
    root->addChild(graph);
    drawMountain(graph, worst->hist, {mw - 16.f, 40.f});
    makeLabel(root, "0%", "chatFont.fnt", 0.55f, {mx + 8.f, 10.f}, {0.f, 0.5f})->setOpacity(150);
    makeLabel(root, "100%", "chatFont.fnt", 0.55f, {mx + mw - 8.f, 10.f}, {1.f, 0.5f})->setOpacity(150);
}

// ---------------------------------------------------------------- Levels

void Hub::onFavFilter(CCObject*) {
    m_favOnly = !m_favOnly;
    this->build();
}

void Hub::onFav(CCObject* sender) {
    int id = static_cast<CCNode*>(sender)->getTag();
    if (auto* l = Store::get().find(id)) {
        bool now = Store::get().toggleFavorite(id, l->name, l->creator);
        if (now) this->react(2, 1.2f);
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
    auto toggler = CCMenuItemToggler::create(
        spr("ludo_check_off.png"_spr, 0.85f), spr("ludo_check_on.png"_spr, 0.85f), this, menu_selector(Hub::onFavFilter));
    toggler->toggle(m_favOnly);
    toggler->setPosition({14.f, area.height - 12.f});
    fm->addChild(toggler);
    makeLabel(root, "Favorites only", "chatFont.fnt", 0.8f, {30.f, area.height - 12.f}, {0.f, 0.5f});

    auto list = st.byRecent();
    if (m_favOnly) {
        list.erase(std::remove_if(list.begin(), list.end(), [](LevelStat* l) { return !l->favorite; }), list.end());
    }
    if (list.empty()) {
        makeLabel(root, m_favOnly ? "No favorites yet. Tap the blossom on a level's page." : "No levels yet.", "chatFont.fnt", 0.9f, {area.width / 2, area.height / 2});
        return;
    }

    float listH = area.height - 30.f;
    auto scroll = ScrollLayer::create({area.width, listH});
    scroll->setPosition({0, 0});
    scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(4.f));
    root->addChild(scroll);

    float W = area.width;
    for (auto* l : list) {
        auto row = CCNode::create();
        row->setContentSize({W, 34.f});

        auto bg = card({W, 34.f}, 225);
        bg->setPosition({W / 2, 17.f});
        row->addChild(bg);

        std::string name = l->name.empty() ? fmt::format("Level {}", l->id) : l->name;
        makeLabel(row, shorten(name, 30), "bigFont.fnt", 0.42f, {10.f, 23.f}, {0.f, 0.5f});
        makeLabel(row, fmt::format("{}   {} tries   {} deaths", formatDuration(l->seconds), l->attempts, l->deaths),
            "chatFont.fnt", 0.7f, {10.f, 9.f}, {0.f, 0.5f})->setOpacity(190);

        // best percent as a brush stroke
        float barW = 60.f, barX = W - 168.f;
        auto track = nine("ludo_pill_off.png"_spr, {barW, 6.f}, NineSlice::Insets{2.f, 8.f, 2.f, 8.f});
        track->setPosition({barX + barW / 2, 9.f});
        track->setOpacity(160);
        row->addChild(track);
        if (l->best > 0) {
            float fw = std::max(8.f, barW * static_cast<float>(l->best) / 100.f);
            auto fillBar = nine("ludo_pill_on.png"_spr, {fw, 6.f}, NineSlice::Insets{2.f, 8.f, 2.f, 8.f});
            fillBar->setPosition({barX + fw / 2, 9.f});
            row->addChild(fillBar);
        }
        makeLabel(row, fmt::format("{}%", l->best), "chatFont.fnt", 0.55f, {barX + barW + 4.f, 9.f}, {0.f, 0.5f})->setOpacity(170);

        // red seal for levels you have beaten
        if (l->completions > 0) {
            auto stamp = spr("ludo_stamp.png"_spr, 0.85f);
            stamp->setRotation(-6.f);
            stamp->setPosition({W - 86.f, 17.f});
            row->addChild(stamp);
        }

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        row->addChild(menu);

        auto favBtn = CCMenuItemSpriteExtra::create(
            spr(l->favorite ? "ludo_blossom_on.png"_spr : "ludo_blossom_off.png"_spr, 0.85f), this, menu_selector(Hub::onFav));
        favBtn->setTag(l->id);
        favBtn->setPosition({W - 54.f, 17.f});
        menu->addChild(favBtn);

        auto goBtn = CCMenuItemSpriteExtra::create(spr("ludo_ic_open.png"_spr, 0.8f), this, menu_selector(Hub::onOpenLevel));
        goBtn->setTag(l->id);
        goBtn->setPosition({W - 20.f, 17.f});
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
            if (!Popup::init(290.f, 150.f, "ludo_panel.png"_spr)) return false;
            hub = h;
            auto sz = m_size;
            this->setCloseButtonSpr(spr("ludo_ic_x.png"_spr));
            heading(m_mainLayer, "New collection", 0.5f, {sz.width / 2, sz.height - 24.f});
            auto brush = spr("ludo_brush.png"_spr, 0.8f);
            brush->setPosition({sz.width / 2, sz.height - 38.f});
            m_mainLayer->addChild(brush);
            input = TextInput::create(220.f, "Name", "bigFont.fnt");
            input->setMaxCharCount(30);
            input->setPosition({sz.width / 2, sz.height / 2 - 6.f});
            m_mainLayer->addChild(input);
            auto menu = CCMenu::create();
            menu->setPosition({0, 0});
            m_mainLayer->addChild(menu);
            auto btn = CCMenuItemSpriteExtra::create(pill("Create", 96.f, true), this, menu_selector(NamePopup::onCreate));
            btn->setPosition({sz.width / 2, 26.f});
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
            hub->react(2, 1.4f);
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
            this->react(3, 1.4f);
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
    auto add = CCMenuItemSpriteExtra::create(pill("New collection", 132.f, true, 24.f), this, menu_selector(Hub::onNewCollection));
    add->setPosition({area.width - 66.f, area.height - 12.f});
    menu->addChild(add);

    if (cols.empty()) {
        heading(root, "No collections yet", 0.5f, {area.width / 2, area.height / 2 + 10.f});
        makeLabel(root, "Make one here, then add levels from a level's page.", "chatFont.fnt", 0.8f, {area.width / 2, area.height / 2 - 14.f});
        return;
    }

    float listH = area.height - 30.f;
    auto scroll = ScrollLayer::create({area.width, listH});
    scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(4.f));
    root->addChild(scroll);

    float W = area.width;
    for (size_t i = 0; i < cols.size(); i++) {
        auto row = CCNode::create();
        row->setContentSize({W, 34.f});
        auto bg = nine("ludo_scroll_row.png"_spr, {W, 34.f}, NineSlice::Insets{10.f, 26.f, 10.f, 26.f});
        bg->setPosition({W / 2, 17.f});
        row->addChild(bg);

        double secs = 0;
        for (int id : cols[i].ids) if (auto* l = Store::get().find(id)) secs += l->seconds;
        heading(row, shorten(cols[i].name, 26), 0.36f, {34.f, 22.f}, {0.f, 0.5f});
        makeLabel(row, fmt::format("{} levels   {} played", cols[i].ids.size(), formatDuration(secs)), "chatFont.fnt", 0.7f, {34.f, 9.f}, {0.f, 0.5f})
            ->setOpacity(190);

        auto m = CCMenu::create();
        m->setPosition({0, 0});
        row->addChild(m);
        auto view = CCMenuItemSpriteExtra::create(pill("Open", 62.f, true, 24.f), this, menu_selector(Hub::onViewCollection));
        view->setTag(static_cast<int>(i));
        view->setPosition({W - 82.f, 17.f});
        m->addChild(view);
        auto delBtn = CCMenuItemSpriteExtra::create(spr("ludo_ic_x.png"_spr, 0.8f), this, menu_selector(Hub::onDeleteCollection));
        delBtn->setTag(static_cast<int>(i));
        delBtn->setPosition({W - 36.f, 17.f});
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
    auto back = CCMenuItemSpriteExtra::create(spr("ludo_ic_back.png"_spr, 0.9f), this, menu_selector(Hub::onBack));
    back->setPosition({14.f, area.height - 12.f});
    menu->addChild(back);
    heading(root, shorten(col.name, 30), 0.44f, {34.f, area.height - 12.f}, {0.f, 0.5f});

    if (col.ids.empty()) {
        makeLabel(root, "Empty. Add levels from a level's page (scroll button).", "chatFont.fnt", 0.8f, {area.width / 2, area.height / 2});
        return;
    }

    float listH = area.height - 30.f;
    auto scroll = ScrollLayer::create({area.width, listH});
    scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(4.f));
    root->addChild(scroll);

    float W = area.width;
    for (int id : col.ids) {
        auto* l = Store::get().find(id);
        auto row = CCNode::create();
        row->setContentSize({W, 34.f});
        auto bg = card({W, 34.f}, 225);
        bg->setPosition({W / 2, 17.f});
        row->addChild(bg);

        std::string name = (l && !l->name.empty()) ? l->name : fmt::format("Level {}", id);
        makeLabel(row, shorten(name, 30), "bigFont.fnt", 0.42f, {10.f, 23.f}, {0.f, 0.5f});
        if (l) {
            makeLabel(row, fmt::format("{}   {} tries   {} deaths   best {}%", formatDuration(l->seconds), l->attempts, l->deaths, l->best),
                "chatFont.fnt", 0.7f, {10.f, 9.f}, {0.f, 0.5f})->setOpacity(190);
            if (l->completions > 0) {
                auto stamp = spr("ludo_stamp.png"_spr, 0.85f);
                stamp->setRotation(-6.f);
                stamp->setPosition({W - 86.f, 17.f});
                row->addChild(stamp);
            }
        }
        auto m = CCMenu::create();
        m->setPosition({0, 0});
        row->addChild(m);
        auto goBtn = CCMenuItemSpriteExtra::create(spr("ludo_ic_open.png"_spr, 0.8f), this, menu_selector(Hub::onOpenLevel));
        goBtn->setTag(id);
        goBtn->setPosition({W - 54.f, 17.f});
        m->addChild(goBtn);
        auto xBtn = CCMenuItemSpriteExtra::create(spr("ludo_ic_x.png"_spr, 0.8f), this, menu_selector(Hub::onRemoveFromCollection));
        xBtn->setTag(id);
        xBtn->setPosition({W - 20.f, 17.f});
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
    this->setCloseButtonSpr(spr("ludo_ic_x.png"_spr));
    auto size = m_size;
    heading(m_mainLayer, "Add to collection", 0.5f, {size.width / 2, size.height - 24.f});
    auto brush = spr("ludo_brush.png"_spr, 0.9f);
    brush->setPosition({size.width / 2, size.height - 39.f});
    m_mainLayer->addChild(brush);

    // make sure the level exists in the store so the collection can show its name
    Store::get().level(levelID, name, creator);
    Store::get().saveIfDirty();

    auto& cols = Store::get().collections();
    if (cols.empty()) {
        makeLabel(m_mainLayer, "No collections yet.", "goldFont.fnt", 0.7f, {size.width / 2, size.height / 2 + 10.f});
        makeLabel(m_mainLayer, "Create one in the Ludo hub", "chatFont.fnt", 0.8f, {size.width / 2, size.height / 2 - 12.f});
        makeLabel(m_mainLayer, "(button on the main menu).", "chatFont.fnt", 0.8f, {size.width / 2, size.height / 2 - 28.f});
        return true;
    }

    auto scroll = ScrollLayer::create({size.width - 40.f, size.height - 84.f});
    scroll->setPosition({20.f, 20.f});
    scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(4.f));
    m_mainLayer->addChild(scroll);
    for (size_t i = 0; i < cols.size(); i++) {
        auto row = CCNode::create();
        row->setContentSize({size.width - 40.f, 32.f});
        auto bg = card({size.width - 40.f, 32.f}, 200);
        bg->setPosition({(size.width - 40.f) / 2, 16.f});
        row->addChild(bg);
        makeLabel(row, shorten(cols[i].name, 24), "bigFont.fnt", 0.42f, {38.f, 16.f}, {0.f, 0.5f});
        auto m = CCMenu::create();
        m->setPosition({0, 0});
        row->addChild(m);
        auto t = CCMenuItemToggler::create(spr("ludo_check_off.png"_spr), spr("ludo_check_on.png"_spr), this, menu_selector(PickerPopup::onToggle));
        t->toggle(Store::get().inCollection(i, levelID));
        t->setTag(static_cast<int>(i));
        t->setPosition({20.f, 16.f});
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
