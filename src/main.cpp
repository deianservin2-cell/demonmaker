#include <Geode/Geode.hpp>
#include <Geode/modify/CreatorLayer.hpp>
using namespace geode::prelude;

static const char* PART_NAMES[4] = {"body", "horns", "eyes", "mouth"};
static const char* PART_LABELS[4] = {"Body", "Horns", "Eyes", "Mouth"};
static const int PART_COUNT[4] = {5, 4, 4, 4};

class DemonMakerLayer : public CCLayer {
public:
    int m_sel[4] = {0, 0, 0, 0};
    TextInput* m_name = nullptr;
    CCNode* m_preview = nullptr;
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
        title->setPosition({win.width / 2, win.height - 28.f});
        this->addChild(title);

        // Panel oscuro a la izquierda con los 4 selectores
        auto panel = CCLayerColor::create(ccc4(0, 40, 130, 200), 190.f, 215.f);
        panel->setPosition({20.f, win.height / 2 - 112.f});
        this->addChild(panel, -1);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});

        for (int i = 0; i < 4; i++) {
            float y = win.height / 2 + 75.f - i * 50.f;
            addArrow(menu, true, 45.f, y, i, -1);
            addArrow(menu, false, 185.f, y, i, +1);
            auto lbl = CCLabelBMFont::create(PART_LABELS[i], "bigFont.fnt");
            lbl->setScale(0.45f);
            lbl->setPosition({115.f, y});
            this->addChild(lbl);
        }

        float rx = win.width * 0.70f;

        m_preview = CCNode::create();
        m_preview->setPosition({rx, win.height / 2 + 25.f});
        this->addChild(m_preview);
        redraw();

        m_name = TextInput::create(190.f, "Difficulty name");
        m_name->setPosition({rx - 20.f, 55.f});
        m_name->setMaxCharCount(16);
        this->addChild(m_name);

        auto saveBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Save"), this,
            menu_selector(DemonMakerLayer::onSave));
        saveBtn->setPosition({rx + 105.f, 55.f});
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
        m_info->setScale(0.35f);
        m_info->setPosition({win.width / 2, 15.f});
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
        int n = PART_COUNT[which];
        m_sel[which] = (m_sel[which] + dir + n) % n;
        redraw();
    }

    void redraw() {
        m_preview->removeAllChildren();
        for (int i = 0; i < 4; i++) {
            auto file = fmt::format("{}_{}.png", PART_NAMES[i], m_sel[i]);
            std::string path = Mod::get()->expandSpriteName(file.c_str());
            auto spr = CCSprite::create(path.c_str());
            if (!spr) continue;
            spr->setScale(0.6f);
            m_preview->addChild(spr);
        }
    }

    void onSave(CCObject*) {
        std::string name = m_name->getString();
        if (name.empty()) {
            FLAlertLayer::create("Demon Maker", "Escribi un nombre primero.", "OK")->show();
            return;
        }
        int count = Mod::get()->getSavedValue<int>("count", 0);
        std::string data = fmt::format("{}|{}|{}|{}|{}", name,
            m_sel[0], m_sel[1], m_sel[2], m_sel[3]);
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
