#include <Geode/Geode.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <map>
#include <vector>
using namespace geode::prelude;

// ===================== DATOS =====================
static const char* PART_NAMES[4] = {"body", "horns", "eyes", "mouth"};
static const char* PART_LABELS[4] = {"Body", "Horns", "Eyes", "Mouth"};
static const int PART_COUNT[4] = {5, 4, 4, 4};
// centro vertical (en pixeles de la imagen de 256) de cada pieza, para centrarla en el selector
static const float PART_CY[4] = {140.f, 60.f, 138.f, 185.f};

static int g_claiming = -1; // indice del demon que se esta reclamando (-1 = ninguno)

struct Demon {
    std::string name;
    int p[4];
};

static std::vector<Demon> loadDemons() {
    std::vector<Demon> out;
    int count = Mod::get()->getSavedValue<int>("count", 0);
    for (int i = 0; i < count; i++) {
        std::string s = Mod::get()->getSavedValue<std::string>(fmt::format("demon_{}", i), "");
        Demon d;
        d.name = "?";
        for (int k = 0; k < 4; k++) d.p[k] = 0;
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t pos = s.find('|', start);
            if (pos == std::string::npos) { parts.push_back(s.substr(start)); break; }
            parts.push_back(s.substr(start, pos - start));
            start = pos + 1;
        }
        if (!parts.empty() && !parts[0].empty()) d.name = parts[0];
        for (int k = 0; k < 4; k++) {
            if ((int)parts.size() > k + 1) d.p[k] = std::atoi(parts[k + 1].c_str());
        }
        out.push_back(d);
    }
    return out;
}

static int saveDemon(Demon const& d) {
    int count = Mod::get()->getSavedValue<int>("count", 0);
    std::string data = fmt::format("{}|{}|{}|{}|{}", d.name, d.p[0], d.p[1], d.p[2], d.p[3]);
    Mod::get()->setSavedValue<std::string>(fmt::format("demon_{}", count), data);
    Mod::get()->setSavedValue<int>("count", count + 1);
    return count;
}

static std::map<int, int> loadClaims() {
    std::map<int, int> m;
    std::string s = Mod::get()->getSavedValue<std::string>("claims", "");
    size_t start = 0;
    while (start < s.size()) {
        size_t end = s.find(',', start);
        if (end == std::string::npos) end = s.size();
        std::string tok = s.substr(start, end - start);
        size_t c = tok.find(':');
        if (c != std::string::npos) {
            m[std::atoi(tok.substr(0, c).c_str())] = std::atoi(tok.substr(c + 1).c_str());
        }
        start = end + 1;
    }
    return m;
}

static void saveClaims(std::map<int, int> const& m) {
    std::string s;
    for (auto const& kv : m) {
        if (!s.empty()) s += ",";
        s += fmt::format("{}:{}", kv.first, kv.second);
    }
    Mod::get()->setSavedValue<std::string>("claims", s);
}

static int claimedCount(int demonIdx) {
    int n = 0;
    for (auto const& kv : loadClaims()) if (kv.second == demonIdx) n++;
    return n;
}

// ===================== AYUDAS DE DIBUJO =====================
static CCSprite* makeSprite(std::string const& file) {
    std::string path = Mod::get()->expandSpriteName(file.c_str());
    return CCSprite::create(path.c_str());
}

// Cara completa dentro de un cuadrado de "canvas" puntos de lado (centrada).
static CCNode* makeFace(const int p[4], float canvas) {
    auto node = CCNode::create();
    node->setContentSize({canvas, canvas});
    node->setAnchorPoint({0.5f, 0.5f});
    for (int i = 0; i < 4; i++) {
        auto s = makeSprite(fmt::format("{}_{}.png", PART_NAMES[i], p[i]));
        if (!s) continue;
        s->setScale(canvas / s->getContentSize().width);
        s->setPosition({canvas / 2, canvas / 2});
        node->addChild(s);
    }
    return node;
}

static CCNode* makeFaceOf(Demon const& d, float canvas) {
    return makeFace(d.p, canvas);
}

static cocos2d::extension::CCScale9Sprite* makePanel(const char* file, float w, float h) {
    CCSize sz = {32.f, 32.f};
    if (auto tmp = makeSprite(file)) sz = tmp->getContentSize();
    std::string path = Mod::get()->expandSpriteName(file);
    auto s = cocos2d::extension::CCScale9Sprite::create(path.c_str(), CCRect(0, 0, sz.width, sz.height));
    if (s) s->setContentSize({w, h});
    return s;
}

// Boton rectangular con texto
static CCNode* makeButton(const char* text, const char* bg, float w, float h) {
    auto node = CCNode::create();
    node->setContentSize({w, h});
    node->setAnchorPoint({0.5f, 0.5f});
    if (auto panel = makePanel(bg, w, h)) {
        panel->setPosition({w / 2, h / 2});
        node->addChild(panel);
    }
    auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
    float sc = 0.55f;
    lbl->setScale(sc);
    float maxW = w - 18.f;
    if (lbl->getContentSize().width * sc > maxW) lbl->setScale(maxW / lbl->getContentSize().width);
    lbl->setPosition({w / 2, h / 2 + 1.f});
    node->addChild(lbl, 1);
    return node;
}

// Boton redondo con icono
static CCNode* makeRound(const char* bg, const char* icon, float size) {
    auto node = CCNode::create();
    node->setContentSize({size, size});
    node->setAnchorPoint({0.5f, 0.5f});
    if (auto b = makeSprite(bg)) {
        b->setScale(size / b->getContentSize().width);
        b->setPosition({size / 2, size / 2});
        node->addChild(b);
    }
    if (icon) {
        if (auto i = makeSprite(icon)) {
            i->setScale(size * 0.72f / i->getContentSize().width);
            i->setPosition({size / 2, size / 2});
            node->addChild(i, 1);
        }
    }
    return node;
}

static void addBackground(CCLayer* layer) {
    auto win = CCDirector::get()->getWinSize();
    auto bg = CCSprite::create("GJ_gradientBG.png");
    if (bg) {
        bg->setAnchorPoint({0.f, 0.f});
        bg->setScaleX(win.width / bg->getContentSize().width);
        bg->setScaleY(win.height / bg->getContentSize().height);
        bg->setColor({0, 102, 255});
        layer->addChild(bg, -5);
    } else {
        layer->addChild(CCLayerColor::create(ccc4(0, 102, 255, 255), win.width, win.height), -5);
    }
}

static CCMenuItemSpriteExtra* makeBackButton(CCObject* target, SEL_MenuHandler cb) {
    auto spr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
    return CCMenuItemSpriteExtra::create(spr, target, cb);
}

// Busca el sprite de dificultad de GD dentro de un nodo
static GJDifficultySprite* findDiff(CCNode* root) {
    if (!root) return nullptr;
    if (auto d = typeinfo_cast<GJDifficultySprite*>(root)) return d;
    auto kids = root->getChildren();
    if (!kids) return nullptr;
    for (auto k : CCArrayExt<CCNode*>(kids)) {
        if (auto r = findDiff(k)) return r;
    }
    return nullptr;
}

// Si el nivel esta reclamado, reemplaza la cara de dificultad por la tuya
static void applyFace(GJGameLevel* level, GJDifficultySprite* diff, bool withName) {
    if (!level || !diff || !diff->getParent()) return;
    auto parent = diff->getParent();
    parent->removeChildByTag(9001);
    parent->removeChildByTag(9002);

    auto claims = loadClaims();
    auto it = claims.find(level->m_levelID);
    if (it == claims.end()) return;
    auto demons = loadDemons();
    if (it->second < 0 || it->second >= (int)demons.size()) return;
    auto const& d = demons[it->second];

    diff->setVisible(false);
    float canvas = diff->getContentSize().width * diff->getScale() * 1.35f;
    auto face = makeFaceOf(d, canvas);
    face->setPosition(diff->getPosition());
    face->setTag(9001);
    parent->addChild(face, diff->getZOrder() + 1);

    if (withName) {
        auto lbl = CCLabelBMFont::create(d.name.c_str(), "bigFont.fnt");
        lbl->setScale(0.4f);
        lbl->setPosition({diff->getPositionX(), diff->getPositionY() - canvas * 0.55f});
        lbl->setTag(9002);
        parent->addChild(lbl, diff->getZOrder() + 1);
    }
}

// ===================== PANTALLA: LAST STEP =====================
class ClaimLayer : public CCLayer {
public:
    int m_idx = 0;
    TextInput* m_input = nullptr;

    static ClaimLayer* create(int idx) {
        auto l = new ClaimLayer();
        l->m_idx = idx;
        if (l->init()) { l->autorelease(); return l; }
        delete l;
        return nullptr;
    }

    static CCScene* scene(int idx) {
        auto s = CCScene::create();
        s->addChild(ClaimLayer::create(idx));
        return s;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        g_claiming = -1;
        auto win = CCDirector::get()->getWinSize();
        addBackground(this);

        float cx = win.width / 2;
        float cy = win.height / 2;

        if (auto panel = makePanel("panel_brown.png", 380.f, 120.f)) {
            panel->setPosition({cx, cy + 62.f});
            this->addChild(panel);
        }
        if (auto sil = makeSprite("silhouette.png")) {
            sil->setScale(70.f / sil->getContentSize().width);
            sil->setPosition({cx - 140.f, cy + 62.f});
            this->addChild(sil);
        }
        auto title = CCLabelBMFont::create("Last Step", "goldFont.fnt");
        title->setScale(0.8f);
        title->setPosition({cx + 30.f, cy + 92.f});
        this->addChild(title);
        auto t1 = CCLabelBMFont::create("Choose a free level...", "chatFont.fnt");
        t1->setPosition({cx + 30.f, cy + 64.f});
        this->addChild(t1);
        auto t2 = CCLabelBMFont::create("(without custom difficulty)", "chatFont.fnt");
        t2->setPosition({cx + 30.f, cy + 42.f});
        this->addChild(t2);

        // tu demon
        auto demons = loadDemons();
        if (m_idx >= 0 && m_idx < (int)demons.size()) {
            auto face = makeFaceOf(demons[m_idx], 70.f);
            face->setPosition({cx - 150.f, cy - 40.f});
            this->addChild(face);
            auto nm = CCLabelBMFont::create(demons[m_idx].name.c_str(), "bigFont.fnt");
            nm->setScale(0.4f);
            nm->setPosition({cx - 150.f, cy - 82.f});
            this->addChild(nm);
        }

        m_input = TextInput::create(230.f, "Level name or ID");
        m_input->setPosition({cx + 20.f, cy - 15.f});
        m_input->setMaxCharCount(30);
        this->addChild(m_input);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});

        auto searchBtn = CCMenuItemSpriteExtra::create(
            makeButton("Search", "rbtn_green.png", 110.f, 38.f), this,
            menu_selector(ClaimLayer::onSearch));
        searchBtn->setPosition({cx - 25.f, cy - 62.f});
        menu->addChild(searchBtn);

        auto idBtn = CCMenuItemSpriteExtra::create(
            makeButton("Claim ID", "rbtn_green.png", 110.f, 38.f), this,
            menu_selector(ClaimLayer::onClaimId));
        idBtn->setPosition({cx + 95.f, cy - 62.f});
        menu->addChild(idBtn);

        auto back = makeBackButton(this, menu_selector(ClaimLayer::onBack));
        back->setPosition({25.f, win.height - 25.f});
        menu->addChild(back);
        this->addChild(menu);

        auto hint = CCLabelBMFont::create(
            "Search a level, open it and press Claim.", "chatFont.fnt");
        hint->setScale(0.8f);
        hint->setPosition({cx + 20.f, 30.f});
        this->addChild(hint);

        this->setKeypadEnabled(true);
        return true;
    }

    void onSearch(CCObject*) {
        std::string q = m_input->getString();
        if (q.empty()) {
            FLAlertLayer::create("Last Step", "Escribi el nombre o el ID de un nivel.", "OK")->show();
            return;
        }
        g_claiming = m_idx;
        auto obj = GJSearchObject::create(SearchType::Search, q);
        CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, LevelBrowserLayer::scene(obj)));
    }

    void onClaimId(CCObject*) {
        int id = std::atoi(std::string(m_input->getString()).c_str());
        if (id <= 0) {
            FLAlertLayer::create("Last Step", "Escribi un ID de nivel valido (numero).", "OK")->show();
            return;
        }
        auto claims = loadClaims();
        if (claims.count(id)) {
            FLAlertLayer::create("Taken", "Ese nivel ya fue reclamado (Taken).", "OK")->show();
            return;
        }
        claims[id] = m_idx;
        saveClaims(claims);
        FLAlertLayer::create("Last Step", "Nivel reclamado! Ahora tiene tu dificultad.", "OK")->show();
    }

    void onBack(CCObject*) { g_claiming = -1; CCDirector::get()->popScene(); }
    void keyBackClicked() override { g_claiming = -1; CCDirector::get()->popScene(); }
};

// ===================== PANTALLA: MY DEMONS (See more) =====================
class MyDemonsLayer : public CCLayer {
public:
    CREATE_FUNC(MyDemonsLayer);

    static CCScene* scene() {
        auto s = CCScene::create();
        s->addChild(MyDemonsLayer::create());
        return s;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        auto win = CCDirector::get()->getWinSize();
        addBackground(this);

        auto title = CCLabelBMFont::create("My Demons", "goldFont.fnt");
        title->setPosition({win.width / 2, win.height - 28.f});
        this->addChild(title);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});

        auto demons = loadDemons();
        if (demons.empty()) {
            auto none = CCLabelBMFont::create("No demons yet.", "bigFont.fnt");
            none->setScale(0.6f);
            none->setPosition({win.width / 2, win.height / 2});
            this->addChild(none);
        }

        // los ultimos 8, el mas nuevo primero
        int shown = 0;
        for (int i = (int)demons.size() - 1; i >= 0 && shown < 8; i--, shown++) {
            int col = shown % 4;
            int row = shown / 4;
            float x = win.width / 2 + (col - 1.5f) * 134.f;
            float y = win.height / 2 + 45.f - row * 118.f;

            auto cell = CCNode::create();
            cell->setContentSize({126.f, 108.f});
            cell->setAnchorPoint({0.5f, 0.5f});
            if (auto panel = makePanel("panel_brown.png", 126.f, 108.f)) {
                panel->setPosition({63.f, 54.f});
                cell->addChild(panel);
            }
            auto face = makeFaceOf(demons[i], 62.f);
            face->setPosition({63.f, 66.f});
            cell->addChild(face);
            auto nm = CCLabelBMFont::create(demons[i].name.c_str(), "bigFont.fnt");
            nm->setScale(0.38f);
            if (nm->getContentSize().width * 0.38f > 110.f) nm->setScale(110.f / nm->getContentSize().width);
            nm->setPosition({63.f, 26.f});
            cell->addChild(nm);
            auto sub = CCLabelBMFont::create(
                fmt::format("{} claimed - tap", claimedCount(i)).c_str(), "chatFont.fnt");
            sub->setScale(0.6f);
            sub->setPosition({63.f, 11.f});
            cell->addChild(sub);

            auto btn = CCMenuItemSpriteExtra::create(cell, this, menu_selector(MyDemonsLayer::onPick));
            btn->setTag(i);
            btn->setPosition({x, y});
            menu->addChild(btn);
        }

        auto back = makeBackButton(this, menu_selector(MyDemonsLayer::onBack));
        back->setPosition({25.f, win.height - 25.f});
        menu->addChild(back);
        this->addChild(menu);

        this->setKeypadEnabled(true);
        return true;
    }

    void onPick(CCObject* sender) {
        int idx = static_cast<CCNode*>(sender)->getTag();
        CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, ClaimLayer::scene(idx)));
    }

    void onBack(CCObject*) { CCDirector::get()->popScene(); }
    void keyBackClicked() override { CCDirector::get()->popScene(); }
};

// ===================== PANTALLA: CREATE A DEMON =====================
class DemonMakerLayer : public CCLayer {
public:
    int m_sel[4] = {0, 0, 0, 0};
    TextInput* m_name = nullptr;
    CCNode* m_faceHolder = nullptr;
    CCNode* m_rowHolders[4] = {nullptr, nullptr, nullptr, nullptr};

    CREATE_FUNC(DemonMakerLayer);

    static CCScene* scene() {
        auto s = CCScene::create();
        s->addChild(DemonMakerLayer::create());
        return s;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        auto win = CCDirector::get()->getWinSize();
        addBackground(this);

        auto title = CCLabelBMFont::create("Create a Demon", "goldFont.fnt");
        title->setPosition({win.width / 2, win.height - 28.f});
        this->addChild(title);

        // Panel izquierdo con los 4 selectores
        if (auto panel = makePanel("panel_dark.png", 120.f, 224.f)) {
            panel->setPosition({95.f, win.height / 2 - 6.f});
            this->addChild(panel, -1);
        }

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});

        for (int i = 0; i < 4; i++) {
            float y = win.height / 2 + 72.f - i * 50.f;
            addArrow(menu, true, 38.f, y, i, -1);
            addArrow(menu, false, 152.f, y, i, +1);
            m_rowHolders[i] = CCNode::create();
            m_rowHolders[i]->setPosition({95.f, y});
            this->addChild(m_rowHolders[i], 1);
        }

        float rx = win.width * 0.70f;

        // Panel de vista previa (derecha, arriba)
        if (auto panel = makePanel("panel_dark.png", 250.f, 150.f)) {
            panel->setPosition({rx, win.height / 2 + 38.f});
            this->addChild(panel, -1);
        }
        m_faceHolder = CCNode::create();
        m_faceHolder->setPosition({rx, win.height / 2 + 38.f});
        this->addChild(m_faceHolder, 1);

        // Nombre
        m_name = TextInput::create(240.f, "Enter Difficulty name");
        m_name->setPosition({rx, 98.f});
        m_name->setMaxCharCount(16);
        this->addChild(m_name);

        // 3 botones redondos: subir, exportar, ver mas
        auto upBtn = CCMenuItemSpriteExtra::create(
            makeRound("cbtn_green.png", "icon_upload.png", 56.f), this,
            menu_selector(DemonMakerLayer::onUpload));
        upBtn->setPosition({rx - 85.f, 48.f});
        menu->addChild(upBtn);

        auto exBtn = CCMenuItemSpriteExtra::create(
            makeRound("cbtn_brown.png", "icon_export.png", 56.f), this,
            menu_selector(DemonMakerLayer::onExport));
        exBtn->setPosition({rx, 48.f});
        menu->addChild(exBtn);

        auto moreBtn = CCMenuItemSpriteExtra::create(
            makeRound("cbtn_green.png", "icon_chat.png", 56.f), this,
            menu_selector(DemonMakerLayer::onMore));
        moreBtn->setPosition({rx + 85.f, 48.f});
        menu->addChild(moreBtn);

        auto moreLbl = CCLabelBMFont::create("See more!", "goldFont.fnt");
        moreLbl->setScale(0.45f);
        moreLbl->setPosition({rx + 85.f, 12.f});
        this->addChild(moreLbl);

        auto back = makeBackButton(this, menu_selector(DemonMakerLayer::onBack));
        back->setPosition({25.f, win.height - 25.f});
        menu->addChild(back);

        this->addChild(menu);
        redraw();
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
        // cara grande
        m_faceHolder->removeAllChildren();
        auto face = makeFace(m_sel, 175.f);
        face->setPosition({0.f, 0.f});
        m_faceHolder->addChild(face);

        // iconos de cada fila
        for (int i = 0; i < 4; i++) {
            m_rowHolders[i]->removeAllChildren();
            auto s = makeSprite(fmt::format("{}_{}.png", PART_NAMES[i], m_sel[i]));
            if (!s) continue;
            float canvas = 72.f;
            s->setScale(canvas / s->getContentSize().width);
            s->setPosition({0.f, (PART_CY[i] - 128.f) / 256.f * canvas});
            m_rowHolders[i]->addChild(s);
        }
    }

    Demon currentDemon(std::string const& name) {
        Demon d;
        d.name = name;
        for (int k = 0; k < 4; k++) d.p[k] = m_sel[k];
        return d;
    }

    std::string cleanName() {
        std::string name = m_name->getString();
        for (auto& c : name) if (c == '|') c = ' ';
        return name;
    }

    void onUpload(CCObject*) {
        std::string name = cleanName();
        if (name.empty()) {
            FLAlertLayer::create("Demon Maker", "Escribi un nombre para la dificultad primero.", "OK")->show();
            return;
        }
        int idx = saveDemon(currentDemon(name));
        CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, ClaimLayer::scene(idx)));
    }

    void onExport(CCObject*) {
        std::string name = cleanName();
        if (name.empty()) name = "demon";
        std::string safe;
        for (char c : name) safe += (std::isalnum((unsigned char)c) ? c : '_');

        auto rt = CCRenderTexture::create(256, 300);
        if (!rt) {
            FLAlertLayer::create("Export", "No se pudo crear la imagen.", "OK")->show();
            return;
        }
        auto face = makeFace(m_sel, 256.f);
        face->setPosition({128.f, 168.f});
        auto lbl = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
        lbl->setScale(0.9f);
        if (lbl->getContentSize().width * 0.9f > 240.f) lbl->setScale(240.f / lbl->getContentSize().width);
        lbl->setPosition({128.f, 32.f});

        rt->beginWithClear(0.f, 0.f, 0.f, 0.f);
        face->visit();
        lbl->visit();
        rt->end();

        std::string file = fmt::format("demon_{}.png", safe);
        bool ok = rt->saveToFile(file.c_str(), kCCImageFormatPNG);
        std::string where = CCFileUtils::sharedFileUtils()->getWritablePath();
        if (ok) {
            FLAlertLayer::create("Export",
                fmt::format("Imagen guardada como {}\nen: {}", file, where).c_str(), "OK")->show();
        } else {
            FLAlertLayer::create("Export", "No se pudo guardar la imagen.", "OK")->show();
        }
    }

    void onMore(CCObject*) {
        CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, MyDemonsLayer::scene()));
    }

    void onBack(CCObject*) { CCDirector::get()->popScene(); }
    void keyBackClicked() override { CCDirector::get()->popScene(); }
};

// ===================== BOTON "DEMON" EN EL MENU ONLINE =====================
class $modify(DemonCreatorLayer, CreatorLayer) {
    bool init() {
        if (!CreatorLayer::init()) return false;
        g_claiming = -1;
        auto win = CCDirector::get()->getWinSize();

        auto node = CCNode::create();
        node->setContentSize({70.f, 70.f});
        node->setAnchorPoint({0.5f, 0.5f});
        if (auto bg = makeSprite("sbtn_green.png")) {
            bg->setScale(70.f / bg->getContentSize().width);
            bg->setPosition({35.f, 35.f});
            node->addChild(bg);
        }
        int p[4] = {1, 0, 0, 0};
        auto face = makeFace(p, 56.f);
        face->setPosition({35.f, 40.f});
        node->addChild(face, 1);
        auto lbl = CCLabelBMFont::create("Demon", "bigFont.fnt");
        lbl->setScale(0.35f);
        lbl->setPosition({35.f, 9.f});
        node->addChild(lbl, 2);

        auto btn = CCMenuItemSpriteExtra::create(node, this,
            menu_selector(DemonCreatorLayer::onDemonMaker));
        auto menu = CCMenu::create();
        menu->addChild(btn);
        menu->setPosition({win.width - 45.f, 55.f});
        this->addChild(menu, 10);
        return true;
    }

    void onDemonMaker(CCObject*) {
        CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, DemonMakerLayer::scene()));
    }
};

// ===================== PAGINA DE UN NIVEL: Claim / Taken + tu cara =====================
class $modify(DemonInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;
        applyFace(level, m_difficultySprite, true);

        if (g_claiming >= 0) {
            auto win = CCDirector::get()->getWinSize();
            bool taken = loadClaims().count(level->m_levelID) > 0;
            auto btnNode = taken
                ? makeButton("Taken", "rbtn_red.png", 110.f, 38.f)
                : makeButton("Claim", "rbtn_green.png", 110.f, 38.f);
            auto btn = CCMenuItemSpriteExtra::create(btnNode, this,
                menu_selector(DemonInfoLayer::onDemonClaim));
            auto menu = CCMenu::create();
            menu->addChild(btn);
            menu->setPosition({75.f, 38.f});
            menu->setTag(9100);
            this->addChild(menu, 50);
        }
        return true;
    }

    void onDemonClaim(CCObject*) {
        if (g_claiming < 0 || !m_level) return;
        auto claims = loadClaims();
        int id = m_level->m_levelID;
        if (claims.count(id)) {
            FLAlertLayer::create("Taken", "Ese nivel ya fue reclamado (Taken).", "OK")->show();
            return;
        }
        claims[id] = g_claiming;
        saveClaims(claims);
        g_claiming = -1;
        this->removeChildByTag(9100);
        applyFace(m_level, m_difficultySprite, true);
        FLAlertLayer::create("Claimed!", "Ahora este nivel tiene tu dificultad.", "OK")->show();
    }
};

// ===================== LISTAS DE NIVELES: tu cara en las celdas =====================
class $modify(DemonCell, LevelCell) {
    void loadFromLevel(GJGameLevel* level) {
        LevelCell::loadFromLevel(level);
        applyFace(level, findDiff(this), false);
    }
};
