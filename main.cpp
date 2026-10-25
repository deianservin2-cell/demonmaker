#include <Geode/Geode.hpp>
#include <Geode/modify/CreatorLayer.hpp>
using namespace geode::prelude;

static const ccColor3B PALETTE[] = {
    {160, 60, 200}, {200, 50, 50}, {60, 120, 230}, {60, 190, 90},
    {240, 170, 40}, {240, 240, 240}, {40, 40, 40}
};
constexpr int PALETTE_N = 7;

class DemonMakerLayer : public CCLayer {
public:
    int m_body = 0;
    int m_eyes = 5;
    TextInput* m_name = nullptr;
    CCDrawNode* m_preview = nullptr;
    CCLabelBMFont* m_info = nullptr;

    CREATE_FUNC(DemonMakerLayer);

    static CCScene* scene() {
        auto s = CCScene::create();
        s->addChild(DemonMakerLayer::create());
        return s;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        auto win = CCDirector::get()->getWinSize();

        auto bg = CCLayerColor::create(ccc4(0, 102, 255, 255), win.width, win.height);
        this->addChild(bg, -2);

        auto title = CCLabelBMFont::create("Create a Demon", "goldFont.fnt");
        title->setPosition({win.width / 2, win.height - 30.f});
        this->addChild(title);

        m_preview = CCDrawNode::create();
        this->addChild(m_preview);
        redraw();

        m_name = TextInput::create(200.f, "Difficulty name");
        m_name->setPosition({win.width / 2, 60.f});
        m_name->setMaxCharCount(16);
        this->addChild(m_name);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        float cx = win.width / 2;
        float cy = win.height / 2;

        addArrow(menu, true,  cx - 110.f, cy + 40.f, 0, -1);
        addArrow(menu, false, cx + 110.f, cy + 40.f, 0, +1);
        addArrow(menu, true,  cx - 110.f, cy - 10.f, 1, -1);
        addArrow(menu, false, cx + 110.f, cy - 10.f, 1, +1);

        auto lbl1 = CCLabelBMFont::create("Body", "bigFont.fnt");
        lbl1->setScale(0.4f);
        lbl1->setPosition({cx - 160.f, cy + 40.f});
        this->addChild(lbl1);
        auto lbl2 = CCLabelBMFont::create("Eyes", "bigFont.fnt");
        lbl2->setScale(0.4f);
        lbl2->setPosition({cx - 160.f, cy - 10.f});
        this->addChild(lbl2);

        auto saveBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Save"), this,
            menu_selector(DemonMakerLayer::onSave));
        saveBtn->setPosition({cx + 150.f, 60.f});
        menu->addChild(saveBtn);

        auto backSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
        auto backBtn = CCMenuItemSpriteExtra::create(
            backSpr, this, menu_selector(DemonMakerLayer::onBack));
        backBtn->setPosition({25.f, win.height - 25.f});
        menu->addChild(backBtn);

        this->addChild(menu);

        int count = Mod::get()->getSavedValue<int>("count", 0);
        m_info = CCLabelBMFont::create(
            fmt::format("Saved demons: {}", count).c_str(), "bigFont.fnt");
        m_info->setScale(0.4f);
        m_info->setPosition({win.width / 2, 25.f});
        this->addChild(m_info);

        this->setKeypadEnabled(true);
        return true;
    }

    void addArrow(CCMenu* menu, bool left, float x, float y, int which, int dir) {
        auto spr = CCSprite::createWithSpriteFrameName("navArrowBtn_001.png");
        spr->setFlipX(left);
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(DemonMakerLayer::onArrow));
        btn->setTag(which * 10 + (dir > 0 ? 2 : 1));
        btn->setPosition({x, y});
        menu->addChild(btn);
    }

    void onArrow(CCObject* sender) {
        int tag = static_cast<CCNode*>(sender)->getTag();
        int which = tag / 10;
        int dir = (tag % 10 == 2) ? 1 : -1;
        int& target = (which == 0) ? m_body : m_eyes;
        target = (target + dir + PALETTE_N) % PALETTE_N;
        redraw();
    }

    void redraw() {
        auto win = CCDirector::get()->getWinSize();
        CCPoint c = {win.width / 2, win.height / 2 + 15.f};
        m_preview->clear();
        auto body = ccc4FFromccc3B(PALETTE[m_body]);
        auto eyes = ccc4FFromccc3B(PALETTE[m_eyes]);
        m_preview->drawDot(c, 45.f, body);
        m_preview->drawDot({c.x - 16.f, c.y + 8.f}, 8.f, eyes);
        m_preview->drawDot({c.x + 16.f, c.y + 8.f}, 8.f, eyes);
        m_preview->drawSegment({c.x - 18.f, c.y - 18.f}, {c.x + 18.f, c.y - 18.f},
                               3.f, ccc4f(1.f, 1.f, 1.f, 1.f));
    }

    void onSave(CCObject*) {
        std::string name = m_name->getString();
        if (name.empty()) {
            FLAlertLayer::create("Demon Maker", "Escribi un nombre primero.", "OK")->show();
            return;
        }
        int count = Mod::get()->getSavedValue<int>("count", 0);
        std::string data = fmt::format("{}|{}|{}", name, m_body, m_eyes);
        Mod::get()->setSavedValue<std::string>(fmt::format("demon_{}", count), data);
        Mod::get()->setSavedValue<int>("count", count + 1);
        m_info->setString(fmt::format("Saved demons: {}", count + 1).c_str());
        FLAlertLayer::create("Demon Maker", "Dificultad guardada.", "OK")->show();
    }

    void onBack(CCObject*) {
        CCDirector::get()->popScene();
    }

    void keyBackClicked() override {
        CCDirector::get()->popScene();
    }
};

class $modify(DemonCreatorLayer, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;
        auto win = CCDirector::get()->getWinSize();
        auto btn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Demon"), this,
            menu_selector(DemonCreatorLayer::onDemonMaker));
        auto menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition({win.width - 60.f, 40.f});
        this->addChild(menu, 10);
        return true;
    }

    void onDemonMaker(CCObject*) {
        auto scene = DemonMakerLayer::scene();
        CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, scene));
    }
};
