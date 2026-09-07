#include "spell_selection_panel.h"
#include "text_renderer.h"
#include <android/log.h>

#define LOG_TAG "SpellSelectionPanel"
#define LOGD_SP(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI_SP(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

SpellSelectionPanel::SpellSelectionPanel(const std::string& name)
    : UIPanel(name) {
}

SpellSelectionPanel::~SpellSelectionPanel() {
    cleanup();
}

bool SpellSelectionPanel::initialize() {
    if (!UIPanel::initialize()) {
        return false;
    }

    setTitle("Spells");
    setTitleBarColor(glm::vec4(0.2f, 0.2f, 0.4f, 0.9f));
    setCloseButtonVisible(true);
    setDraggable(true);

    LOGI_SP("SpellSelectionPanel initialized");
    return true;
}

void SpellSelectionPanel::cleanup() {
    clearSpellButtons();
    UIPanel::cleanup();
}

void SpellSelectionPanel::setSpells(const std::vector<std::shared_ptr<Spell>>& spellList) {
    spells = spellList;
    refresh();
}

void SpellSelectionPanel::setTextRenderer(TextRenderer* renderer) {
    textRenderer = renderer;
    refresh();
}

void SpellSelectionPanel::refresh() {
    clearSpellButtons();
    createSpellButtons();
}

// Returns background color for spell school (used as icon substitute)
static glm::vec4 getSchoolColor(MagicSchool school, float alpha = 0.8f) {
    switch (school) {
        case MagicSchool::ALTERATION:  return glm::vec4(0.2f, 0.6f, 0.8f, alpha); // blue
        case MagicSchool::CONJURATION: return glm::vec4(0.5f, 0.2f, 0.7f, alpha); // purple
        case MagicSchool::DESTRUCTION: return glm::vec4(0.8f, 0.2f, 0.1f, alpha); // red
        case MagicSchool::ILLUSION:    return glm::vec4(0.1f, 0.7f, 0.5f, alpha); // teal
        case MagicSchool::MYSTICISM:   return glm::vec4(0.7f, 0.6f, 0.1f, alpha); // gold
        case MagicSchool::RESTORATION: return glm::vec4(0.2f, 0.7f, 0.3f, alpha); // green
        default:                       return glm::vec4(0.3f, 0.3f, 0.5f, alpha); // grey-blue
    }
}

void SpellSelectionPanel::createSpellButtons() {
    if (!textRenderer) return;

    glm::vec2 contentSize = getContentSize();
    // Child positions are relative to the panel; getAbsolutePosition() adds the parent offset.
    glm::vec2 absPos = getAbsolutePosition();
    glm::vec2 contentPos = getContentPosition();
    float y = (contentPos.y - absPos.y) + buttonMargin;

    for (size_t i = 0; i < spells.size(); i++) {
        const auto& spell = spells[i];
        if (!spell) continue;

        auto button = std::make_shared<UIButton>("SpellButton_" + std::to_string(spell->spellId));
        float localContentX = contentPos.x - absPos.x;
        button->setPosition(localContentX + buttonMargin, y);
        button->setSize(contentSize.x - buttonMargin * 2.0f, buttonHeight);
        button->setLabelScale(0.7f);
        button->setTextRenderer(textRenderer);
        // School-color as visual indicator (substitute for icon texture)
        glm::vec4 normalCol  = getSchoolColor(spell->school, 0.75f);
        glm::vec4 pressedCol = getSchoolColor(spell->school, 1.0f);
        button->setNormalColor(normalCol);
        button->setPressedColor(pressedCol);

        // Label: school abbreviation prefix + spell name
        std::string schoolPrefix;
        switch (spell->school) {
            case MagicSchool::ALTERATION:  schoolPrefix = "[ALT] "; break;
            case MagicSchool::CONJURATION: schoolPrefix = "[CON] "; break;
            case MagicSchool::DESTRUCTION: schoolPrefix = "[DES] "; break;
            case MagicSchool::ILLUSION:    schoolPrefix = "[ILL] "; break;
            case MagicSchool::MYSTICISM:   schoolPrefix = "[MYS] "; break;
            case MagicSchool::RESTORATION: schoolPrefix = "[RES] "; break;
            default: schoolPrefix = ""; break;
        }
        std::string spellLabel = schoolPrefix + (spell->nameJa.empty() ? spell->name : spell->nameJa);
        button->setLabel(spellLabel);

        // Capture spell by value for callback
        std::weak_ptr<Spell> weakSpell = spell;
        button->setOnClick([this, weakSpell]() {
            if (auto s = weakSpell.lock()) {
                LOGI("Spell selected: %s", s->name.c_str());
                if (onSpellSelected) {
                    onSpellSelected(s);
                }
                setVisible(false);
            }
        });

        spellButtons.push_back(button);
        addChild(button);
        y += buttonHeight + buttonMargin;
    }

    // Adjust panel height to fit spells if needed
    float minHeight = 150.0f;
    float desiredHeight = y + buttonMargin;
    if (desiredHeight > minHeight) {
        setSize(getSize().x, desiredHeight);
    }
}

void SpellSelectionPanel::clearSpellButtons() {
    for (auto& button : spellButtons) {
        removeChild(button);
    }
    spellButtons.clear();
}