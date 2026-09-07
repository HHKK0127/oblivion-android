#pragma once

// Spell Selection Panel
// Displays a list of known spells and allows the player to select one

#include "ui_panel.h"
#include "ui_button.h"
#include "../game/spell.h"
#include <vector>
#include <memory>
#include <functional>

class SpellSelectionPanel : public UIPanel {
public:
    using SpellSelectedCallback = std::function<void(std::shared_ptr<Spell>)>;

    SpellSelectionPanel(const std::string& name = "SpellSelectionPanel");
    ~SpellSelectionPanel() override;

    bool initialize() override;
    void cleanup() override;

    // Set the list of spells to display
    void setSpells(const std::vector<std::shared_ptr<Spell>>& spellList);

    // Set callback when a spell is selected
    void setOnSpellSelected(SpellSelectedCallback cb) { onSpellSelected = cb; }

    // Set text renderer for buttons
    void setTextRenderer(class TextRenderer* renderer);

    // Refresh the spell list UI
    void refresh();

private:
    std::vector<std::shared_ptr<Spell>> spells;
    std::vector<std::shared_ptr<UIButton>> spellButtons;
    SpellSelectedCallback onSpellSelected;
    class TextRenderer* textRenderer = nullptr;

    float buttonHeight = 60.0f;
    float buttonMargin = 10.0f;

    void createSpellButtons();
    void clearSpellButtons();
};