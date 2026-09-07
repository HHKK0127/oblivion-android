#include "renderer.h"
#include "skinning_shader.h"
#include "texture_loader.h"
#include "imperial_weave.h"
#include "../ui/ui_draw_helper.h"
#include "../assets/bsa_reader.h"
#include "../inventory/item_factory.h"
#include "../physics/physics_manager.h"
// #include "../jni_audio_bridge.h"  // Deferred - requires Java MainActivity
#include <thread>
#include <chrono>

Renderer::Renderer()
    : showLauncher(true), showTitleScreen(false), shouldExit(false),
      screenWidth(1080), screenHeight(1920),
      targetFPS(60), frameTimeThreshold(1000.0f / 60.0f) {
    LOGD("Renderer created with target FPS: %d", targetFPS);
    lastFrameTime = std::chrono::high_resolution_clock::now();
}

Renderer::~Renderer() {
    cleanup();
    LOGD("Renderer destroyed");
}

bool Renderer::init(unsigned int width, unsigned int height) {
    LOGI("===== Renderer::init() START with %ux%u =====", width, height);
    __android_log_print(ANDROID_LOG_ERROR, "Renderer", "SYNC_CHECKPOINT_1: init() called");
    initialized = false;  // Reset initialization flag

    screenWidth = width;
    screenHeight = height;

    LOGI("Renderer initializing: %ux%u", screenWidth, screenHeight);

    try {
        // Initialize localization
        LOGI("Step 1: Calling initLocalization()");
        initLocalization();
        LOGI("Step 1: initLocalization() completed");
        __android_log_print(ANDROID_LOG_ERROR, "Renderer", "SYNC_CHECKPOINT_2: localization done");

        // Initialize game systems
        LOGI("Step 2: Calling initGameSystems()");
        if (!initGameSystems()) {
            LOGE("Failed to initialize game systems");
            __android_log_print(ANDROID_LOG_ERROR, "Renderer", "ERROR_INIT_GAME_SYSTEMS: failed");
            return false;
        }
        LOGI("Step 2: initGameSystems() completed");
        __android_log_print(ANDROID_LOG_ERROR, "Renderer", "SYNC_CHECKPOINT_3: game systems done");

        // Initialize retro filter (post-processing)
        LOGI("Step 3: Initializing RetroFilter");
        retroFilter = std::make_unique<RetroFilter>();
        if (!retroFilter->initialize(screenWidth, screenHeight)) {
            LOGE("Failed to initialize RetroFilter");
            __android_log_print(ANDROID_LOG_ERROR, "Renderer", "ERROR_RETROFILTER: initialization failed");
            return false;
        }
        LOGI("Step 3: RetroFilter initialized");
        __android_log_print(ANDROID_LOG_ERROR, "Renderer", "SYNC_CHECKPOINT_4: retrofilter done");

        // Create test scenario (combat, quests, etc.)
        LOGI("Step 4: Calling createTestScenario()");
        createTestScenario();
        LOGI("Step 4: createTestScenario() completed");
        __android_log_print(ANDROID_LOG_ERROR, "Renderer", "SYNC_CHECKPOINT_5: test scenario done");

        initialized = true;  // Mark as successfully initialized
        LOGI("===== Renderer initialized successfully =====");
        __android_log_print(ANDROID_LOG_ERROR, "Renderer", "SYNC_CHECKPOINT_FINAL: SUCCESS");
        return true;
    } catch (const std::exception& e) {
        LOGE("CRITICAL: Exception during Renderer::init(): %s", e.what());
        __android_log_print(ANDROID_LOG_ERROR, "Renderer", "EXCEPTION_CAUGHT: %s", e.what());
        initialized = false;
        return false;
    } catch (...) {
        LOGE("CRITICAL: Unknown exception during Renderer::init()");
        __android_log_print(ANDROID_LOG_ERROR, "Renderer", "UNKNOWN_EXCEPTION");
        initialized = false;
        return false;
    }
}

void Renderer::resize(unsigned int width, unsigned int height) {
    LOGI("===== Renderer::resize() called with %ux%u =====", width, height);
    screenWidth = width;
    screenHeight = height;
    LOGI("Renderer resized to: %ux%u", screenWidth, screenHeight);

    // Update responsive UI manager
    if (responsiveUIManager) {
        SafeAreaManager::SafeAreaInsets defaultInsets = {0.0f, 0.0f, 0.0f, 0.0f};
        responsiveUIManager->UpdateForScreenChange(
            glm::vec2(static_cast<float>(width), static_cast<float>(height)),
            defaultInsets
        );
        uiScale = responsiveUIManager->GetFontScaler().GetCurrentScale();
        LOGI("ResponsiveUIManager updated, UI Scale: %.2f", uiScale);
    } else {
        // Fallback calculation
        float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        float referenceRatio = 16.0f / 9.0f;
        uiScale = std::max(0.5f, std::min(1.5f, referenceRatio / aspectRatio));
    }
    LOGI("UI Scale factor: %.2f", uiScale);

    // Update TextRenderer with new dimensions - CRITICAL for correct projection
    if (textRenderer) {
        LOGI("TextRenderer exists, calling setScreenSize(%u, %u)", screenWidth, screenHeight);
        textRenderer->setScreenSize(screenWidth, screenHeight);
        LOGI("TextRenderer screen size updated to: %ux%u", screenWidth, screenHeight);
    } else {
        LOGW("WARNING: TextRenderer is NULL in resize()! Dimensions not updated!");
    }

    // Update RetroFilter resolution
    if (retroFilter) {
        retroFilter->setNativeResolution(screenWidth, screenHeight);
        LOGI("RetroFilter resolution updated to: %ux%u", screenWidth, screenHeight);
    }

    // Update Phase 9 UI Framework screen size
    if (uiSystem) {
        uiSystem->setScreenSize(static_cast<int>(screenWidth), static_cast<int>(screenHeight));
        LOGI("UISystem screen size updated to: %ux%u", screenWidth, screenHeight);
        
        // Get safe area adjusted positions
        float joystickRadius = 150.0f * uiScale;
        float joystickX = 200.0f * uiScale;
        float joystickY = screenHeight - 200.0f * uiScale;
        
        // Apply safe area adjustments if available
        if (responsiveUIManager) {
            auto safeInsets = responsiveUIManager->GetSafeAreaInsets();
            joystickX += safeInsets.left;
            joystickY -= safeInsets.bottom;
        }
        
        if (!joystick) {
            joystick = std::make_shared<UIJoystick>(joystickX, joystickY, joystickRadius);
            joystick->setVisible(false);
            uiSystem->registerComponent(joystick, 100);
        } else {
            joystick->setPosition(joystickX, joystickY);
        }

        // Setup combat buttons on right side of screen (bottom-right)
        float btnSize = 120.0f * uiScale;
        float btnMargin = 20.0f * uiScale;
        float btnX = screenWidth - btnSize - btnMargin;
        
        // Apply safe area adjustments
        if (responsiveUIManager) {
            auto safeInsets = responsiveUIManager->GetSafeAreaInsets();
            btnX -= safeInsets.right;
        }

        // Attack button (bottom-right)
        if (!attackButton) {
            attackButton = std::make_shared<UIButton>("AttackButton");
            attackButton->setPosition(btnX, screenHeight - btnSize - btnMargin);
            attackButton->setSize(btnSize, btnSize);
            attackButton->setLabel("ATK");
            attackButton->setLabelScale(0.8f * uiScale);
            attackButton->setNormalColor(glm::vec4(0.8f, 0.2f, 0.2f, 0.7f));
            attackButton->setPressedColor(glm::vec4(1.0f, 0.1f, 0.1f, 0.9f));
            attackButton->setTextRenderer(textRenderer.get());
            attackButton->setOnClick([this]() {
                LOGD("ATK button callback fired! playerController=%p combatManager=%p worldManager=%p",
                     playerController.get(), combatManager.get(), worldManager.get());
                if (playerController) {
                    playerController->attack();
                }
                // Find nearest enemy and attack
                if (combatManager && worldManager) {
                    glm::vec3 playerPos = worldManager->getPlayerPosition();
                    auto nearestEnemy = combatManager->findNearestEnemyToPlayer(playerPos, 30.0f);
                    if (nearestEnemy) {
                        // Attack the nearest enemy (player ID = 1, weapon ID = 0 for unarmed)
                        combatManager->playerAttack(1, nearestEnemy->npcId, 0);
                        LOGD("Attacking nearest enemy: %s", nearestEnemy->name.c_str());
                    } else {
                        LOGD("No enemy in range");
                    }
                }
            });
            attackButton->setVisible(false); // Hidden until game starts
            uiSystem->registerComponent(attackButton, 100);
        }

        // Block button (above attack)
        if (!blockButton) {
            blockButton = std::make_shared<UIButton>("BlockButton");
            blockButton->setPosition(btnX, screenHeight - btnSize * 2 - btnMargin * 2);
            blockButton->setSize(btnSize, btnSize);
            blockButton->setLabel("BLK");
            blockButton->setLabelScale(0.8f * uiScale);
            blockButton->setNormalColor(glm::vec4(0.2f, 0.4f, 0.8f, 0.7f));
            blockButton->setPressedColor(glm::vec4(0.3f, 0.5f, 1.0f, 0.9f));
            blockButton->setTextRenderer(textRenderer.get());
            blockButton->setOnClick([this]() {
                LOGD("BLK button callback fired! playerController=%p combatManager=%p",
                     playerController.get(), combatManager.get());
                if (playerController && combatManager) {
                    // Toggle combat stance for blocking
                    playerController->toggleCombatStance();
                }
            });
            blockButton->setVisible(false); // Hidden until game starts
            uiSystem->registerComponent(blockButton, 100);
        }

        // Cast spell button (above block)
        if (!castSpellButton) {
            castSpellButton = std::make_shared<UIButton>("CastSpellButton");
            castSpellButton->setPosition(btnX, screenHeight - btnSize * 3 - btnMargin * 3);
            castSpellButton->setSize(btnSize, btnSize);
            castSpellButton->setLabel("MAG");
            castSpellButton->setLabelScale(0.8f * uiScale);
            castSpellButton->setNormalColor(glm::vec4(0.6f, 0.2f, 0.8f, 0.7f));
            castSpellButton->setPressedColor(glm::vec4(0.8f, 0.3f, 1.0f, 0.9f));
            castSpellButton->setTextRenderer(textRenderer.get());
            castSpellButton->setOnClick([this]() {
                // Cast spell using SpellManager
                if (spellManager && combatManager && playerController && playerController->getPlayer()) {
                    LOGD("Cast spell button pressed");
                    auto& player = *playerController->getPlayer();

                    // If no spell selected, show spell selection panel
                    if (!selectedSpell) {
                        // Get player's known spells from Player struct
                        std::vector<std::shared_ptr<Spell>> playerSpells;
                        LOGD("Player equippedSpells count: %zu, knownSpells count: %zu",
                             player.equippedSpells.size(), player.knownSpells.size());
                        for (uint32_t sid : player.equippedSpells) {
                            auto sp = spellManager->getSpell(sid);
                            LOGD("  equippedSpell ID=%u -> %s", sid, sp ? sp->name.c_str() : "NOT FOUND");
                            if (sp) playerSpells.push_back(sp);
                        }
                        if (playerSpells.empty()) {
                            for (uint32_t sid : player.knownSpells) {
                                auto sp = spellManager->getSpell(sid);
                                LOGD("  knownSpell ID=%u -> %s", sid, sp ? sp->name.c_str() : "NOT FOUND");
                                if (sp) playerSpells.push_back(sp);
                            }
                        }

                        if (playerSpells.empty()) {
                            LOGD("No spells available for player");
                            return;
                        }

                        if (spellSelectionPanel) {
                            spellSelectionPanel->setSpells(playerSpells);
                            spellSelectionPanel->setVisible(true);
                            LOGD("Opened spell selection panel with %zu spells", playerSpells.size());
                        }
                        return;
                    }

                    // Find nearest enemy target
                    auto enemy = combatManager->findNearestEnemyToPlayer(worldManager->getPlayerPosition());
                    uint32_t targetId = enemy ? enemy->npcId : 0;

                    // Cast selected spell using player-direct path
                    bool success = spellManager->castPlayerSpell(&player, selectedSpell->spellId, targetId);
                    LOGD("Cast spell %s: %s", selectedSpell->name.c_str(), success ? "success" : "failed");

                    // Play magic sound effect
                    if (audioManager && success) {
                        audioManager->playSound("magic/spell_equip", worldManager->getPlayerPosition());
                    }
                } else {
                    LOGD("Cast spell button pressed - SpellManager not available");
                }
            });
            castSpellButton->setVisible(false); // Hidden until game starts
            uiSystem->registerComponent(castSpellButton, 100);
        }

        // Quick-slot buttons (top-right area, above combat buttons)
        // Place in top-right corner to avoid joystick and DebugHUD overlap
        float slotSize = 70.0f * uiScale;
        float slotMargin = 6.0f * uiScale;
        float slotStartX = screenWidth - (slotSize * 4 + slotMargin * 3) - 20.0f * uiScale;
        float slotY = 20.0f * uiScale; // Top-right, away from DebugHUD (top-left)

        for (int i = 0; i < QUICK_SLOT_COUNT; i++) {
            if (!quickSlotButtons[i]) {
                float slotX = slotStartX + i * (slotSize + slotMargin);
                std::string slotName = "QuickSlot" + std::to_string(i + 1);
                auto btn = std::make_shared<UIButton>(slotName);
                btn->setPosition(slotX, slotY);
                btn->setSize(slotSize, slotSize);
                btn->setLabel("F" + std::to_string(i + 1));
                btn->setLabelScale(0.6f * uiScale);
                btn->setNormalColor(glm::vec4(0.2f, 0.2f, 0.3f, 0.6f));
                btn->setPressedColor(glm::vec4(0.4f, 0.4f, 0.6f, 0.9f));
                btn->setTextRenderer(textRenderer.get());

                // Capture slot index
                btn->setOnClick([this, i]() {
                    if (!playerController || !playerController->getPlayer()) return;
                    auto& player = *playerController->getPlayer();
                    auto spell = player.quickSlotSpells[i];
                    if (!spell) {
                        // Open spell panel to assign to this slot
                        pendingAssignSlot = i;
                        if (spellSelectionPanel && spellManager) {
                            std::vector<std::shared_ptr<Spell>> playerSpells;
                            for (uint32_t sid : player.equippedSpells) {
                                auto sp = spellManager->getSpell(sid);
                                if (sp) playerSpells.push_back(sp);
                            }
                            if (playerSpells.empty()) {
                                for (uint32_t sid : player.knownSpells) {
                                    auto sp = spellManager->getSpell(sid);
                                    if (sp) playerSpells.push_back(sp);
                                }
                            }
                            spellSelectionPanel->setSpells(playerSpells);
                            spellSelectionPanel->setVisible(true);
                        }
                    } else {
                        // Cast spell in this slot
                        selectedSpell = spell;
                        auto enemies = npcManager ? npcManager->getNPCsInArea(
                            playerController->getPlayer()->position, 20.0f) : std::vector<std::shared_ptr<NPC>>{};
                        uint32_t targetId = 0;
                        for (auto& e : enemies) {
                            if (e && e->npcId != 1) { targetId = e->npcId; break; }
                        }
                        if (spellManager) {
                            spellManager->castPlayerSpell(&player, spell->spellId, targetId);
                        }
                        if (audioManager) {
                            audioManager->playSound("magic/spell_equip");
                        }
                    }
                });

                btn->setVisible(false);
                uiSystem->registerComponent(btn, 90);
                quickSlotButtons[i] = btn;
            }
        }
    }

    // Update LauncherScreen layout
    if (launcherScreen) {
        launcherScreen->setScreenSize(static_cast<int>(screenWidth), static_cast<int>(screenHeight));
        LOGI("LauncherScreen screen size updated to: %ux%u", screenWidth, screenHeight);
    }

    // Update TitleScreen layout
    if (titleScreen) {
        titleScreen->setScreenSize(static_cast<int>(screenWidth), static_cast<int>(screenHeight));
        LOGI("TitleScreen screen size updated to: %ux%u", screenWidth, screenHeight);
    }

    // Update QuestUI layout
    if (questUI) {
        questUI->setScreenSize(screenWidth, screenHeight);
    }

    // Update SettingsUI layout
    if (settingsUI) {
        settingsUI->setScreenSize(static_cast<int>(screenWidth), static_cast<int>(screenHeight));
        LOGI("SettingsUI screen size updated to: %ux%u", screenWidth, screenHeight);
    }
}

void Renderer::setTargetFPS(int fps) {
    if (fps <= 0) {
        LOGW("Invalid FPS value: %d, using 60 fps", fps);
        fps = 60;
    }

    targetFPS = fps;
    frameTimeThreshold = 1000.0f / fps;

    LOGI("Target FPS changed to: %d (%.2f ms per frame)", targetFPS, frameTimeThreshold);
}

void Renderer::initLocalization() {
    localizationManager = std::make_unique<LocalizationManager>();
    if (!localizationManager->initialize()) {
        LOGE("Failed to initialize LocalizationManager");
        return;
    }

    LOGI("LocalizationManager initialized");
    localizationManager->logTranslationStats();
}

bool Renderer::initGameSystems() {
    LOGI("=== initGameSystems() called ===");

    // CRITICAL: Get actual viewport dimensions from OpenGL (not JNI parameters)
    // This handles timing issues where render thread starts before onSurfaceChanged
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    unsigned int actualWidth = viewport[2];
    unsigned int actualHeight = viewport[3];
    LOGI("Actual OpenGL viewport: %ux%u", actualWidth, actualHeight);

    if (actualWidth > 0 && actualHeight > 0) {
        screenWidth = actualWidth;
        screenHeight = actualHeight;
        LOGI("Using actual viewport dimensions: %ux%u (instead of init %ux%u)",
             screenWidth, screenHeight,
             ((int)1920), ((int)1080));  // These are hardcoded init values for comparison
    }

    // Initialize Settings Manager
    LOGI("Creating SettingsManager...");
    settingsManager = std::make_unique<SettingsManager>();
    if (!settingsManager->initialize()) {
        LOGE("Failed to initialize SettingsManager");
        return false;
    }
    LOGI("SettingsManager initialized successfully");

    // Initialize Text Renderer (for debug HUD and settings UI)
    LOGI("Creating TextRenderer...");
    textRenderer = std::make_unique<TextRenderer>();
    if (!g_assetManager) {
        LOGE("g_assetManager is null, cannot initialize TextRenderer");
        return false;
    }
    if (!textRenderer->initialize(g_assetManager)) {
        LOGE("Failed to initialize TextRenderer");
        return false;
    }
    textRenderer->setScreenSize(screenWidth, screenHeight);
    LOGI("TextRenderer initialized successfully with size %ux%u", screenWidth, screenHeight);

    // Initialize Phase 9 UI Framework System
    LOGI("Creating UISystem...");
    uiSystem = std::make_unique<UISystem>();
    if (!uiSystem->initialize(textRenderer.get())) {
        LOGE("Failed to initialize UISystem");
    } else {
        uiSystem->setScreenSize(static_cast<int>(screenWidth), static_cast<int>(screenHeight));
        LOGI("UISystem initialized successfully (Phase 9 UI Framework ready)");
    }

    // Initialize Responsive UI Manager
    LOGI("Creating ResponsiveUIManager...");
    responsiveUIManager = std::make_unique<ResponsiveUIManager>();
    responsiveUIManager->Initialize();
    SafeAreaManager::SafeAreaInsets defaultInsets = {0.0f, 0.0f, 0.0f, 0.0f};
    responsiveUIManager->UpdateForScreenChange(
        glm::vec2(static_cast<float>(screenWidth), static_cast<float>(screenHeight)),
        defaultInsets
    );
    LOGI("ResponsiveUIManager initialized successfully");

    // Initialize Floating Combat Text
    LOGI("Creating UIFloatingText...");
    floatingText = std::make_unique<UIFloatingText>();
    if (!floatingText->initialize(textRenderer.get(), static_cast<int>(screenWidth), static_cast<int>(screenHeight))) {
        LOGE("Failed to initialize UIFloatingText");
    } else {
        LOGI("UIFloatingText initialized successfully");
    }

    // Initialize Animation Subscriber
    LOGI("Creating AnimationSubscriber...");
    animSubscriber = std::make_unique<animation::AnimationSubscriber>();
    LOGI("AnimationSubscriber created successfully");

    // Initialize Audio Subscriber
    LOGI("Creating AudioSubscriber...");
    audioSubscriber = std::make_unique<audio::AudioSubscriber>();
    LOGI("AudioSubscriber created successfully");

    // Initialize Spell Selection Panel
    LOGI("Creating SpellSelectionPanel...");
    spellSelectionPanel = std::make_shared<SpellSelectionPanel>();
    if (!spellSelectionPanel->initialize()) {
        LOGE("Failed to initialize SpellSelectionPanel");
    } else {
        spellSelectionPanel->setPosition(screenWidth * 0.2f, screenHeight * 0.2f);
        spellSelectionPanel->setSize(screenWidth * 0.6f, screenHeight * 0.6f);
        spellSelectionPanel->setTextRenderer(textRenderer.get());
        spellSelectionPanel->setVisible(false);
        spellSelectionPanel->setOnSpellSelected([this](std::shared_ptr<Spell> spell) {
            if (pendingAssignSlot >= 0 && pendingAssignSlot < QUICK_SLOT_COUNT) {
                // Assign to quick slot
                if (playerController && playerController->getPlayer()) {
                    playerController->getPlayer()->quickSlotSpells[pendingAssignSlot] = spell;
                }
                // Update button label to spell name
                if (quickSlotButtons[pendingAssignSlot]) {
                    std::string shortName = spell->nameJa.empty() ? spell->name : spell->nameJa;
                    if (shortName.size() > 4) shortName = shortName.substr(0, 4);
                    std::string btnLabel = "F" + std::to_string(pendingAssignSlot + 1) + "\n" + shortName;
                    quickSlotButtons[pendingAssignSlot]->setLabel(btnLabel);
                    // Color by school
                    glm::vec4 col = glm::vec4(0.4f, 0.2f, 0.6f, 0.8f); // default
                    switch (spell->school) {
                        case MagicSchool::DESTRUCTION:  col = glm::vec4(0.7f, 0.15f, 0.1f, 0.8f); break;
                        case MagicSchool::RESTORATION:  col = glm::vec4(0.1f, 0.6f, 0.25f, 0.8f); break;
                        case MagicSchool::CONJURATION:  col = glm::vec4(0.4f, 0.15f, 0.6f, 0.8f); break;
                        case MagicSchool::ALTERATION:   col = glm::vec4(0.1f, 0.5f, 0.7f, 0.8f); break;
                        case MagicSchool::ILLUSION:     col = glm::vec4(0.1f, 0.6f, 0.45f, 0.8f); break;
                        case MagicSchool::MYSTICISM:    col = glm::vec4(0.6f, 0.5f, 0.1f, 0.8f); break;
                        default: break;
                    }
                    quickSlotButtons[pendingAssignSlot]->setNormalColor(col);
                }
                LOGI("Assigned spell '%s' to quick slot %d", spell->name.c_str(), pendingAssignSlot + 1);
                pendingAssignSlot = -1;
            } else {
                selectedSpell = spell;
                LOGI("Spell selected: %s", spell->name.c_str());
            }
        });
        uiSystem->registerComponent(spellSelectionPanel, 200);
        LOGI("SpellSelectionPanel initialized successfully");
    }

    // Initialize Game Console
    LOGI("Creating GameConsole...");
    gameConsole = std::make_unique<GameConsole>();
    if (!gameConsole->initialize(textRenderer.get())) {
        LOGE("Failed to initialize GameConsole");
        return false;
    }
    LOGI("GameConsole initialized successfully");

    // Initialize unified DebugSystem (gesture-based toggling)
    DebugSystem::getInstance().initialize(gameConsole.get());
    LOGI("DebugSystem initialized successfully");

    // Connect GameConsole to game systems via GameSystemRefs
    GameConsole::GameSystemRefs refs;
    refs.getPlayerPos = [this]() -> glm::vec3 {
        if (playerController && playerController->getPlayer()) return playerController->getPlayer()->position;
        return glm::vec3(0.0f, 0.0f, 0.0f);
    };
    refs.setPlayerPos = [this](float x, float y, float z) {
        if (playerController) playerController->setPosition(glm::vec3(x, y, z));
    };
    refs.setHealth = [this](float h) {
        if (playerController && playerController->getPlayer()) playerController->getPlayer()->health = h;
    };
    refs.setMaxHealth = [this](float h) {
        if (playerController && playerController->getPlayer()) playerController->getPlayer()->maxHealth = h;
    };
    refs.setMana = [this](float m) {
        // Magicka not implemented in Player struct yet
    };
    refs.setStamina = [this](float s) {
        if (playerController && playerController->getPlayer()) playerController->getPlayer()->stamina = s;
    };
    refs.setLevel = [this](int l) {
        if (playerController && playerController->getPlayer()) playerController->getPlayer()->setLevel(static_cast<uint32_t>(l));
    };
    refs.addExperience = [this](float xp) {
        if (playerController && playerController->getPlayer()) playerController->getPlayer()->addExperience(xp);
    };
    refs.setSkill = [this](const std::string& name, int level) {
        if (playerController && playerController->getPlayer()) {
            auto& skills = playerController->getPlayer()->skills;
            if (name == "Blade") skills.Blade = level;
            else if (name == "Blunt") skills.Blunt = level;
            else if (name == "Block") skills.Block = level;
            else if (name == "Restoration") skills.Restoration = level;
            else if (name == "Destruction") skills.Destruction = level;
            else if (name == "Alteration") skills.Alteration = level;
            else if (name == "Conjuration") skills.Conjuration = level;
            else if (name == "Illusion") skills.Illusion = level;
            else if (name == "Mysticism") skills.Mysticism = level;
            else if (name == "Marksman") skills.Marksman = level;
            else if (name == "Athletics") skills.Athletics = level;
            else if (name == "Acrobatics") skills.Acrobatics = level;
        }
    };
    refs.setAttribute = [this](const std::string& name, int val) {
        if (playerController && playerController->getPlayer()) {
            auto& attrs = playerController->getPlayer()->attributes;
            if (name == "Strength") attrs.Strength = val;
            else if (name == "Intelligence") attrs.Intelligence = val;
            else if (name == "Willpower") attrs.Willpower = val;
            else if (name == "Agility") attrs.Agility = val;
            else if (name == "Speed") attrs.Speed = val;
            else if (name == "Endurance") attrs.Endurance = val;
            else if (name == "Personality") attrs.Personality = val;
            else if (name == "Luck") attrs.Luck = val;
        }
    };
    refs.maxAllSkills = [this]() {
        if (playerController && playerController->getPlayer()) playerController->getPlayer()->maxOutAllSkills();
    };
    refs.resetPlayerStats = [this]() {
        if (playerController && playerController->getPlayer()) playerController->getPlayer()->resetSkills();
    };
    refs.getPlayerStats = [this]() -> std::string {
        if (playerController && playerController->getPlayer()) {
            auto p = playerController->getPlayer();
            return "HP:" + std::to_string(static_cast<int>(p->health)) + "/" + std::to_string(static_cast<int>(p->maxHealth))
                + " STA:" + std::to_string(static_cast<int>(p->stamina))
                + " LV:" + std::to_string(p->playerLevel);
        }
        return "No player";
    };
    refs.attackNearest = [this]() {
        if (combatManager && playerController && playerController->getPlayer()) {
            auto enemy = combatManager->findNearestEnemyToPlayer(playerController->getPlayer()->position);
            if (enemy) {
                combatManager->playerAttack(1, enemy->npcId, 0);
            }
        }
    };
    refs.blockAction = [this]() {
        // Block handled by combat system
    };
    refs.dodgeAction = [this]() {
        // Dodge handled by combat system
    };
    refs.applyDamageToNpc = [this](uint32_t npcId, float dmg) {
        if (combatManager) {
            auto npc = npcManager ? npcManager->getNPC(npcId) : nullptr;
            if (npc) combatManager->applyDamage(npc, dmg);
        }
    };
    refs.killNpc = [this](uint32_t npcId) {
        if (combatManager && npcManager) {
            auto npc = npcManager->getNPC(npcId);
            if (npc) combatManager->applyDamage(npc, npc->status.currentHealth);
        }
    };
    refs.resurrectNpc = [this](uint32_t npcId) {
        if (npcManager) {
            auto npc = npcManager->getNPC(npcId);
            if (npc) npc->status.currentHealth = npc->status.maxHealth;
        }
    };
    refs.killAllNpcs = [this]() {
        if (combatManager && playerController && playerController->getPlayer()) {
            for (int i = 0; i < 100; ++i) {
                auto enemy = combatManager->findNearestEnemyToPlayer(playerController->getPlayer()->position, 1000.0f);
                if (enemy) {
                    combatManager->applyDamage(enemy, enemy->status.currentHealth);
                } else {
                    break;
                }
            }
        }
    };
    refs.toggleCombatDebug = [this]() {
        // Combat debug not implemented yet
    };
    refs.addItem = [this](uint32_t id, uint32_t qty) {
        if (inventoryManager) {
            Item item;
            item.itemId = id;
            item.name = "DebugItem_" + std::to_string(id);
            inventoryManager->playerAddItem(item, qty);
        }
    };
    refs.removeItem = [this](uint32_t id, uint32_t qty) {
        if (inventoryManager) inventoryManager->playerRemoveItem(id, qty);
    };
    refs.equipItem = [this](uint32_t id) {
        // Equip not implemented in InventoryManager yet
    };
    refs.unequipItem = [this](uint32_t id) {
        // Unequip not implemented in InventoryManager yet
    };
    refs.listInventory = [this]() -> std::string {
        return "Inventory items";
    };
    refs.clearInventory = [this]() {
        // Clear inventory not implemented yet
    };
    refs.setCarryWeight = [this](float w) {
        // Set carry weight not implemented yet
    };
    refs.learnSpell = [this](uint32_t id) {
        // Learn spell not directly available
    };
    refs.castSpellOnTarget = [this](uint32_t spellId, uint32_t targetId) {
        if (spellManager) spellManager->castSpell(1, spellId, targetId);
    };
    refs.equipSpell = [this](uint32_t id) {
        if (spellManager) spellManager->equipSpellToNpc(1, id);
    };
    refs.listSpells = [this]() -> std::string {
        return "Spells available";
    };
    refs.createSpell = [this](const std::string& name, float dmg, float cost) {
        if (spellManager) spellManager->createSpell(name, name, MagicSchool::DESTRUCTION, cost, dmg);
    };
    refs.acceptQuest = [this](uint32_t id) {
        if (questManager) questManager->acceptQuest(id);
    };
    refs.completeQuest = [this](uint32_t id) {
        if (questManager) questManager->completeQuest(id);
    };
    refs.failQuest = [this](uint32_t id) {
        if (questManager) questManager->failQuest(id);
    };
    refs.listQuests = [this]() -> std::string {
        if (questManager) {
            auto quests = questManager->getActiveQuests();
            return "Active quests: " + std::to_string(quests.size());
        }
        return "No quests";
    };
    refs.updateObjective = [this](uint32_t qid, uint32_t obj, uint32_t prog) {
        if (questManager) questManager->updateObjectiveProgress(qid, obj, prog);
    };
    refs.spawnNpcAt = [this](const std::string& name, float x, float y, float z) -> uint32_t {
        if (npcManager) {
            auto npc = npcManager->createNPC(name, glm::vec3(x, y, z));
            if (npc) return npc->npcId;
        }
        return 0;
    };
    refs.setNpcAiState = [this](uint32_t id, const std::string& state) {
        // AI state not directly settable
    };
    refs.setNpcAggression = [this](uint32_t id, float val) {
        // Aggression not directly settable
    };
    refs.calmNpc = [this](uint32_t id) {
        // Calm not directly settable
    };
    refs.listNpcs = [this]() -> std::string {
        if (npcManager) return "NPCs: " + std::to_string(npcManager->getNPCCount());
        return "No NPCs";
    };
    refs.listNearbyNpcs = [this]() -> std::string {
        if (npcManager && playerController && playerController->getPlayer()) {
            auto npcs = npcManager->getNPCsInArea(playerController->getPlayer()->position, 30.0f);
            return "Nearby: " + std::to_string(npcs.size());
        }
        return "No nearby NPCs";
    };
    refs.startDialogueWith = [this](uint32_t id) {
        // DialogueRunner not connected
    };
    refs.selectDialogueTopic = [this](int t) {
        // DialogueRunner not connected
    };
    refs.selectDialogueChoice = [this](int c) {
        // DialogueRunner not connected
    };
    refs.endDialogue = [this]() {
        // DialogueRunner not connected
    };
    refs.setWeather = [this](const std::string& w) {
        // Weather not implemented yet
    };
    refs.setTimeScale = [this](float s) {
        // Time scale not implemented yet
    };
    refs.setTimeOfDay = [this](float h) {
        // Time of day not implemented yet
    };
    refs.loadCell = [this](int32_t x, int32_t y) {
        if (worldManager) worldManager->loadCell(x, y);
    };
    refs.getWorldInfo = [this]() -> std::string {
        return "World info not available";
    };
    // Phase 66: Map debug callbacks
    refs.teleportTo = [this](float x, float y, float z) {
        if (playerController) {
            playerController->setPosition(glm::vec3(x, y, z));
            LOGI("Teleported to (%.1f, %.1f, %.1f)", x, y, z);
        }
    };
    refs.getPlayerPosition = [this]() -> std::string {
        if (!playerController) return "Player controller not available";
        glm::vec3 pos = playerController->getPlayerPosition();
        return "Player Pos: X=" + std::to_string((int)pos.x) +
               " Y=" + std::to_string((int)pos.y) +
               " Z=" + std::to_string((int)pos.z);
    };
    refs.movePlayerRelative = [this](float dx, float dz) {
        if (playerController) {
            glm::vec3 pos = playerController->getPlayerPosition();
            playerController->setPosition(glm::vec3(pos.x + dx, pos.y, pos.z + dz));
        }
    };
    refs.listNearbyCells = [this]() -> std::string {
        if (!playerController) return "Player controller not available";
        glm::vec3 pos = playerController->getPlayerPosition();
        int cx = (int)(pos.x / 4096.0f);
        int cz = (int)(pos.z / 4096.0f);
        std::string result = "=== Nearby Cells ===\n";
        result += "Current: (" + std::to_string(cx) + ", " + std::to_string(cz) + ")\n";
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                result += "  (" + std::to_string(cx+dx) + ", " + std::to_string(cz+dy) + ")";
                if (dx == 0 && dy == 0) result += " [CURRENT]";
                result += "\n";
            }
        }
        return result;
    };
    refs.teleportToCell = [this](int32_t cx, int32_t cz) {
        float x = cx * 4096.0f + 2048.0f;
        float z = cz * 4096.0f + 2048.0f;
        if (playerController) {
            playerController->setPosition(glm::vec3(x, 0.0f, z));
            LOGI("Teleported to cell (%d, %d)", cx, cz);
        }
        if (worldManager) worldManager->loadCell(cx, cz);
    };

    // Phase 67: Performance monitoring callbacks
    refs.getPerformanceStats = [this]() -> std::string {
        if (!performanceMonitor) return "Performance monitor not available";
        auto metrics = performanceMonitor->getMetrics();
        std::string result = "=== Performance Stats ===\n";
        result += "FPS: " + std::to_string((int)metrics.frameMetrics.fps) + "\n";
        result += "Frame Time: " + std::to_string(metrics.frameMetrics.frameTime) + " ms\n";
        result += "CPU Time: " + std::to_string(metrics.frameMetrics.cpuTime) + " ms\n";
        result += "Render Time: " + std::to_string(metrics.frameMetrics.renderTime) + " ms\n";
        result += "Update Time: " + std::to_string(metrics.frameMetrics.updateTime) + " ms\n";
        result += "Dropped Frames: " + std::to_string(metrics.frameMetrics.droppedFrames) + "\n";
        result += "Total Frames: " + std::to_string(metrics.totalFrames) + "\n";
        return result;
    };
    refs.getDetailedPerformance = [this]() -> std::string {
        if (!performanceMonitor) return "Performance monitor not available";
        auto metrics = performanceMonitor->getMetrics();
        std::string result = "=== Detailed Performance ===\n";
        result += "Current FPS: " + std::to_string((int)metrics.frameMetrics.fps) + "\n";
        result += "Avg Frame Time: " + std::to_string(metrics.avgFrameTime) + " ms\n";
        result += "Min Frame Time: " + std::to_string(metrics.minFrameTime) + " ms\n";
        result += "Max Frame Time: " + std::to_string(metrics.maxFrameTime) + " ms\n";
        result += "CPU Usage: " + std::to_string(metrics.cpuUsagePercent) + "%\n";
        result += "\n--- Memory ---\n";
        result += "Heap Size: " + std::to_string(metrics.memoryMetrics.heapSize / 1024 / 1024) + " MB\n";
        result += "Heap Used: " + std::to_string(metrics.memoryMetrics.heapUsed / 1024 / 1024) + " MB\n";
        result += "Heap Free: " + std::to_string(metrics.memoryMetrics.heapFree / 1024 / 1024) + " MB\n";
        result += "Usage: " + std::to_string(metrics.memoryMetrics.heapPercentage) + "%\n";
        result += "Peak Memory: " + std::to_string(metrics.memoryMetrics.peakMemory / 1024 / 1024) + " MB\n";
        return result;
    };
    refs.resetPerformanceStats = [this]() {
        // Performance stats reset not implemented yet
        LOGI("Performance stats reset requested");
    };
    refs.getMemoryStats = [this]() -> std::string {
        if (!performanceMonitor) return "Performance monitor not available";
        auto metrics = performanceMonitor->getMetrics();
        std::string result = "=== Memory Stats ===\n";
        result += "Heap Size: " + std::to_string(metrics.memoryMetrics.heapSize / 1024 / 1024) + " MB\n";
        result += "Heap Used: " + std::to_string(metrics.memoryMetrics.heapUsed / 1024 / 1024) + " MB\n";
        result += "Heap Free: " + std::to_string(metrics.memoryMetrics.heapFree / 1024 / 1024) + " MB\n";
        result += "Usage: " + std::to_string(metrics.memoryMetrics.heapPercentage) + "%\n";
        result += "Peak Memory: " + std::to_string(metrics.memoryMetrics.peakMemory / 1024 / 1024) + " MB\n";
        result += "Allocations: " + std::to_string(metrics.memoryMetrics.allocationCount) + "\n";
        return result;
    };
    refs.getDrawCallStats = [this]() -> std::string {
        // Draw call stats would need renderer integration
        return "Draw Call Stats:\n  (Not implemented yet)";
    };

    // Phase 68: NPC debug callbacks
    refs.listAllNpcs = [this]() -> std::string {
        if (!npcManager) return "NPC Manager not available";
        auto npcs = npcManager->getAllNPCs();
        std::string result = "=== NPCs (" + std::to_string(npcs.size()) + ") ===\n";
        for (const auto& npc : npcs) {
            result += "[" + std::to_string(npc->npcId) + "] " + npc->name;
            result += " HP:" + std::to_string((int)npc->status.currentHealth);
            result += " Pos:(" + std::to_string((int)npc->position.x) + "," + std::to_string((int)npc->position.y) + "," + std::to_string((int)npc->position.z) + ")";
            result += " AI:" + std::to_string((int)npc->aiState) + "\n";
        }
        return result;
    };
    refs.getNpcInfo = [this](uint32_t npcId) -> std::string {
        if (!npcManager) return "NPC Manager not available";
        auto npc = npcManager->getNPC(npcId);
        if (!npc) return "NPC not found: " + std::to_string(npcId);
        std::string result = "=== NPC Info ===\n";
        result += "ID: " + std::to_string(npc->npcId) + "\n";
        result += "Name: " + npc->name + "\n";
        result += "Race: " + npc->race + "\n";
        result += "Class: " + npc->class_ + "\n";
        result += "HP: " + std::to_string((int)npc->status.currentHealth) + "/" + std::to_string((int)npc->status.maxHealth) + "\n";
        result += "Mana: " + std::to_string((int)npc->status.currentMana) + "/" + std::to_string((int)npc->status.maxMana) + "\n";
        result += "Stamina: " + std::to_string((int)npc->status.stamina) + "/" + std::to_string((int)npc->status.maxStamina) + "\n";
        result += "Position: (" + std::to_string(npc->position.x) + ", " + std::to_string(npc->position.y) + ", " + std::to_string(npc->position.z) + ")\n";
        result += "AI State: " + std::to_string((int)npc->aiState) + "\n";
        result += "In Combat: " + std::string(npc->inCombat ? "Yes" : "No") + "\n";
        result += "Move Speed: " + std::to_string(npc->moveSpeed) + "\n";
        return result;
    };
    refs.spawnNpc = [this](const std::string& name, float x, float y, float z) {
        if (!npcManager) return;
        auto npc = npcManager->createNPC(name, glm::vec3(x, y, z));
        if (npc) {
            LOGI("Spawned NPC '%s' at (%f, %f, %f)", name.c_str(), x, y, z);
        }
    };
    refs.killAllNpcs = [this]() {
        if (!npcManager) return;
        auto npcs = npcManager->getAllNPCs();
        for (auto& npc : npcs) {
            npc->status.currentHealth = 0;
        }
        LOGI("Killed all NPCs");
    };
    refs.toggleNpcAi = [this](bool enabled) {
        // Toggle NPC AI updates
        LOGI("NPC AI %s", enabled ? "enabled" : "disabled");
    };
    refs.getNpcCount = [this]() -> std::string {
        if (!npcManager) return "NPC Manager not available";
        return "NPC Count: " + std::to_string(npcManager->getNPCCount());
    };
    refs.setNpcSpeed = [this](float speed) {
        if (!npcManager) return;
        auto npcs = npcManager->getAllNPCs();
        for (auto& npc : npcs) {
            npc->moveSpeed = speed;
        }
        LOGI("Set all NPC speed to %f", speed);
    };

    // Phase 69: Combat debug callbacks
    refs.getCombatStats = [this]() -> std::string {
        if (!combatManager) return "Combat Manager not available";
        std::string result = "=== Combat Stats ===\n";
        result += "Active Combats: " + std::to_string(combatManager->getActiveCombatCount()) + "\n";
        return result;
    };
    refs.getActiveCombats = [this]() -> std::string {
        if (!combatManager) return "Combat Manager not available";
        auto combats = combatManager->getActiveCombatsList();
        std::string result = "=== Active Combats ===\n";
        for (const auto& combat : combats) {
            result += combat + "\n";
        }
        if (combats.empty()) result += "No active combats\n";
        return result;
    };
    refs.attackNearestEnemy = [this]() {
        if (!combatManager || !npcManager || !playerController) return;
        auto enemy = combatManager->findNearestEnemyToPlayer(playerController->getPlayerPosition());
        if (enemy) {
            combatManager->playerAttack(0, enemy->npcId, 0);
            LOGI("Attacking nearest enemy: %s", enemy->name.c_str());
        }
    };
    refs.toggleCombatOverlay = [this](bool enabled) {
        LOGI("Combat overlay %s", enabled ? "enabled" : "disabled");
    };
    refs.setDamageMultiplier = [this](float multiplier) {
        LOGI("Damage multiplier set to %f", multiplier);
    };
    refs.toggleInvincibility = [this](bool enabled) {
        LOGI("Invincibility %s", enabled ? "enabled" : "disabled");
    };
    refs.setPlayerDamage = [this](float minDmg, float maxDmg) {
        LOGI("Player damage range set to %f - %f", minDmg, maxDmg);
    };

    // Phase 70: Magic debug callbacks
    refs.listAllSpells = [this]() -> std::string {
        if (!spellManager) return "Spell Manager not available";
        // SpellManager doesn't have a getAllSpells method, so we'll return a placeholder
        return "=== Spells ===\n  (Use spell IDs to query specific spells)";
    };
    refs.getSpellInfo = [this](uint32_t spellId) -> std::string {
        if (!spellManager) return "Spell Manager not available";
        auto spell = spellManager->getSpell(spellId);
        if (!spell) return "Spell not found: " + std::to_string(spellId);
        std::string result = "=== Spell Info ===\n";
        result += "ID: " + std::to_string(spell->spellId) + "\n";
        result += "Name: " + spell->name + "\n";
        result += "Name (JP): " + spell->nameJa + "\n";
        result += "School: " + std::to_string((int)spell->school) + "\n";
        result += "Mana Cost: " + std::to_string(spell->manaCost) + "\n";
        result += "Base Damage: " + std::to_string(spell->baseDamage) + "\n";
        result += "Effects: " + std::to_string(spell->effects.size()) + "\n";
        return result;
    };
    refs.castSpellAtNearest = [this](uint32_t spellId) {
        if (!spellManager || !combatManager) return;
        auto enemy = combatManager->findNearestEnemyToPlayer(playerController->getPlayerPosition());
        if (enemy) {
            spellManager->castPlayerSpell(nullptr, spellId, enemy->npcId);
            LOGI("Cast spell %d at %s", spellId, enemy->name.c_str());
        }
    };
    refs.setSpellDamageMultiplier = [this](float multiplier) {
        LOGI("Spell damage multiplier set to %f", multiplier);
    };
    refs.toggleInfiniteMana = [this](bool enabled) {
        LOGI("Infinite mana %s", enabled ? "enabled" : "disabled");
    };
    refs.getPlayerSpells = [this]() -> std::string {
        if (!spellManager || !playerController) return "Spell Manager not available";
        auto player = playerController->getPlayer();
        if (!player) return "Player not available";
        auto spells = spellManager->getNpcSpells(player->playerId);
        std::string result = "=== Player Spells (" + std::to_string(spells.size()) + ") ===\n";
        for (const auto& spell : spells) {
            result += "[" + std::to_string(spell->spellId) + "] " + spell->name;
            result += " Cost:" + std::to_string((int)spell->manaCost);
            result += " Dmg:" + std::to_string((int)spell->baseDamage) + "\n";
        }
        return result;
    };
    refs.teachSpellToPlayer = [this](uint32_t spellId) {
        if (!spellManager || !playerController) return;
        auto player = playerController->getPlayer();
        if (!player) return;
        spellManager->teachSpellToNpc(player->playerId, spellId);
        LOGI("Taught spell %d to player", spellId);
    };

    // Phase 71: Inventory debug callbacks
    refs.listPlayerInventory = [this]() -> std::string {
        if (!inventoryManager) return "Inventory Manager not available";
        auto inventory = inventoryManager->getPlayerInventory();
        if (!inventory) return "Player inventory not available";
        std::string result = "=== Player Inventory ===\n";
        result += "Weight: " + std::to_string((int)inventory->getTotalWeight()) + "/" + std::to_string((int)inventory->MAX_WEIGHT) + " kg\n";
        uint32_t usedSlots = 0;
        for (uint32_t i = 0; i < inventory->MAX_SLOTS; ++i) {
            const auto& slot = inventory->getSlot(i);
            if (!slot.isEmpty()) usedSlots++;
        }
        result += "Slots: " + std::to_string(usedSlots) + "/" + std::to_string(inventory->MAX_SLOTS) + "\n\n";
        for (uint32_t i = 0; i < inventory->MAX_SLOTS; ++i) {
            const auto& slot = inventory->getSlot(i);
            if (!slot.isEmpty()) {
                result += "[" + std::to_string(i) + "] " + slot.item.name;
                result += " x" + std::to_string(slot.quantity);
                result += " (" + std::to_string(slot.item.weight) + " kg)\n";
            }
        }
        return result;
    };
    refs.getItemInfo = [this](uint32_t itemId) -> std::string {
        if (!inventoryManager) return "Inventory Manager not available";
        auto item = inventoryManager->getItemTemplate(itemId);
        if (!item) return "Item not found: " + std::to_string(itemId);
        std::string result = "=== Item Info ===\n";
        result += "ID: " + std::to_string(item->itemId) + "\n";
        result += "Name: " + item->name + "\n";
        result += "Type: " + std::to_string((int)item->type) + "\n";
        result += "Weight: " + std::to_string(item->weight) + " kg\n";
        result += "Value: " + std::to_string(item->value) + " gold\n";
        return result;
    };
    refs.addItemToPlayer = [this](uint32_t itemId, uint32_t quantity) {
        if (!inventoryManager) return;
        auto item = inventoryManager->getItemTemplate(itemId);
        if (!item) return;
        for (uint32_t i = 0; i < quantity; ++i) {
            inventoryManager->playerAddItem(*item, 1);
        }
        LOGI("Added %d of item %d to player", quantity, itemId);
    };
    refs.removeItemFromPlayer = [this](uint32_t itemId, uint32_t quantity) {
        if (!inventoryManager) return;
        inventoryManager->playerRemoveItem(itemId, quantity);
        LOGI("Removed %d of item %d from player", quantity, itemId);
    };
    refs.clearInventory = [this]() {
        if (!inventoryManager) return;
        auto inventory = inventoryManager->getPlayerInventory();
        if (inventory) {
            inventory->clear();
            LOGI("Inventory cleared");
        }
    };
    refs.getInventoryWeight = [this]() -> std::string {
        if (!inventoryManager) return "Inventory Manager not available";
        auto inventory = inventoryManager->getPlayerInventory();
        if (!inventory) return "Player inventory not available";
        return "Weight: " + std::to_string((int)inventory->getTotalWeight()) + "/" + std::to_string((int)inventory->MAX_WEIGHT) + " kg";
    };
    refs.setCarryCapacity = [this](float capacity) {
        LOGI("Carry capacity set to %f kg", capacity);
    };

    // Phase 72: Quest debug enhanced callbacks
    refs.getActiveQuestList = [this]() -> std::string {
        if (!questManager) return "Quest Manager not available";
        auto activeQuests = questManager->getActiveQuests();
        if (activeQuests.empty()) return "No active quests";
        std::string result = "=== Active Quests (" + std::to_string(activeQuests.size()) + ") ===\n";
        for (const auto& quest : activeQuests) {
            result += "[" + std::to_string(quest->questId) + "] " + quest->title + "\n";
            result += "  State: " + std::to_string((int)quest->state) + "\n";
            result += "  Objectives: " + std::to_string(quest->getCompletedObjectiveCount()) + "/" + std::to_string(quest->objectives.size()) + "\n";
        }
        return result;
    };
    refs.getQuestDetails = [this](uint32_t questId) -> std::string {
        if (!questManager) return "Quest Manager not available";
        auto quest = questManager->getQuest(questId);
        if (!quest) return "Quest not found: " + std::to_string(questId);
        std::string result = "=== Quest Details ===\n";
        result += "ID: " + std::to_string(quest->questId) + "\n";
        result += "Title: " + quest->title + "\n";
        result += "Description: " + quest->description + "\n";
        result += "State: " + std::to_string((int)quest->state) + "\n";
        result += "Giver NPC: " + std::to_string(quest->giverNpcId) + "\n";
        result += "\nObjectives:\n";
        for (const auto& obj : quest->objectives) {
            result += "  [" + std::to_string(obj.objectiveId) + "] " + obj.description;
            result += " (" + std::to_string(obj.currentProgress) + "/" + std::to_string(obj.targetProgress) + ")";
            if (obj.isCompleted()) result += " [DONE]";
            result += "\n";
        }
        return result;
    };
    refs.resetQuest = [this](uint32_t questId) {
        if (!questManager) return;
        auto quest = questManager->getQuest(questId);
        if (!quest) return;
        quest->state = QuestState::PENDING;
        for (auto& obj : quest->objectives) {
            obj.currentProgress = 0;
            obj.state = QuestObjectiveState::PENDING;
        }
        LOGI("Quest %d reset to PENDING", questId);
    };
    refs.getQuestRewardInfo = [this](uint32_t questId) -> std::string {
        if (!questManager) return "Quest Manager not available";
        auto quest = questManager->getQuest(questId);
        if (!quest) return "Quest not found: " + std::to_string(questId);
        std::string result = "=== Quest Reward ===\n";
        result += "Gold: " + std::to_string(quest->reward.goldAmount) + "\n";
        result += "XP: " + std::to_string(quest->reward.experiencePoints) + "\n";
        if (!quest->reward.itemRewards.empty()) {
            result += "Items:\n";
            for (const auto& item : quest->reward.itemRewards) {
                result += "  - " + item + "\n";
            }
        }
        return result;
    };
    refs.completeAllObjectives = [this](uint32_t questId) {
        if (!questManager) return;
        auto quest = questManager->getQuest(questId);
        if (!quest) return;
        for (auto& obj : quest->objectives) {
            obj.currentProgress = obj.targetProgress;
            obj.state = QuestObjectiveState::COMPLETED;
        }
        LOGI("All objectives completed for quest %d", questId);
    };

    // Phase 73: Dialogue debug enhanced callbacks
    refs.getDialogueState = [this]() -> std::string {
        return "=== Dialogue State ===\nDialogue system: Available\nUse 'talk <npcId>' to start dialogue";
    };
    refs.getDialogueTopics = [this]() -> std::string {
        return "=== Available Topics ===\nUse 'dialoguetopics' after starting dialogue";
    };
    refs.getDialogueChoices = [this]() -> std::string {
        return "=== Current Choices ===\nUse 'dialoguechoices' after starting dialogue";
    };
    refs.getDialogueHistory = [this]() -> std::string {
        return "=== Dialogue History ===\nNo dialogue history available";
    };
    refs.resetDialogue = [this]() {
        // DialogueRunner not connected - placeholder
        LOGI("Dialogue reset requested");
    };

    // Phase 74: World debug enhanced callbacks
    refs.getWorldInfoDetailed = [this]() -> std::string {
        if (!worldManager) return "World Manager not available";
        std::string result = "=== World Info (Detailed) ===\n";
        auto& state = worldManager->getWorldState();
        result += "Time: " + std::to_string(state.timeOfDay) + "h\n";
        result += "Day: " + std::to_string(state.dayCount) + "\n";
        result += "Player Pos: (" + std::to_string(state.playerPosition.x) + ", " + std::to_string(state.playerPosition.y) + ", " + std::to_string(state.playerPosition.z) + ")\n";
        result += "Active Cells: " + std::to_string(worldManager->getActiveCells().size()) + "\n";
        result += "Total Cells: " + std::to_string(worldManager->getAllCellsMap().size()) + "\n";
        result += "World Items: " + std::to_string(worldManager->getWorldItems().size()) + "\n";
        result += "Load Radius: " + std::to_string(worldManager->getCellLoadRadius()) + "\n";
        result += "Unload Radius: " + std::to_string(worldManager->getCellUnloadRadius()) + "\n";
        return result;
    };
    refs.getCellDetails = [this](int32_t cellX, int32_t cellY) -> std::string {
        if (!worldManager) return "World Manager not available";
        auto cell = worldManager->getCellByCoord(cellX, cellY);
        if (!cell) return "Cell not found: " + std::to_string(cellX) + "," + std::to_string(cellY);
        std::string result = "=== Cell Details ===\n";
        result += "ID: " + std::to_string(cell->cellId) + "\n";
        result += "Coord: (" + std::to_string(cellX) + ", " + std::to_string(cellY) + ")\n";
        result += "Name: " + cell->cellName + "\n";
        result += "Type: " + std::to_string((int)cell->cellType) + "\n";
        return result;
    };
    refs.getActiveCellsList = [this]() -> std::string {
        if (!worldManager) return "World Manager not available";
        auto& activeCells = worldManager->getActiveCells();
        if (activeCells.empty()) return "No active cells";
        std::string result = "=== Active Cells (" + std::to_string(activeCells.size()) + ") ===\n";
        for (const auto& cell : activeCells) {
            result += "[" + std::to_string(cell->cellId) + "] " + cell->cellName + "\n";
        }
        return result;
    };
    refs.getWorldItemsList = [this]() -> std::string {
        if (!worldManager) return "World Manager not available";
        auto& items = worldManager->getWorldItems();
        if (items.empty()) return "No world items";
        std::string result = "=== World Items (" + std::to_string(items.size()) + ") ===\n";
        for (const auto& item : items) {
            result += "[" + std::to_string(item->itemId) + "] " + item->itemName + "\n";
        }
        return result;
    };
    refs.getDoorInfo = [this]() -> std::string {
        if (!worldManager) return "World Manager not available";
        auto* doorMgr = worldManager->getDoorManager();
        if (!doorMgr) return "Door Manager not available";
        return "Door Manager: Active";
    };

    refs.saveGameSlot = [this](uint32_t slot) {
        if (saveManager) saveManager->saveGame(slot);
    };
    refs.loadGameSlot = [this](uint32_t slot) {
        if (saveManager) saveManager->loadGame(slot);
    };
    refs.quickSave = [this]() {
        if (saveManager) saveManager->quickSave();
    };
    refs.quickLoad = [this]() {
        if (saveManager) saveManager->quickLoad();
    };
    refs.listSaveSlots = [this]() -> std::string {
        if (saveManager) {
            auto slots = saveManager->getSaveSlots();
            return "Save slots: " + std::to_string(slots.size());
        }
        return "No saves";
    };
    refs.openMenu = [this](const std::string& menu) {
        // TODO: Implement menu opening
    };
    refs.closeMenu = [this]() {
        // TODO: Implement menu closing
    };
    // Phase 65: Extended Debug callbacks
    refs.toggleWireframe = [this]() {
        wireframeMode = !wireframeMode;
        LOGI("Wireframe mode: %s", wireframeMode ? "ON" : "OFF");
    };
    refs.toggleAabb = [this]() {
        aabbVisualization = !aabbVisualization;
        LOGI("AABB visualization: %s", aabbVisualization ? "ON" : "OFF");
    };
    refs.toggleNpcOverlay = [this]() {
        npcOverlay = !npcOverlay;
        LOGI("NPC overlay: %s", npcOverlay ? "ON" : "OFF");
    };
    refs.toggleTouchTrail = [this]() {
        touchTrail = !touchTrail;
        LOGI("Touch trail: %s", touchTrail ? "ON" : "OFF");
    };

    // Sound callbacks
    refs.playBgm = [this]() {
        if (audioManager) audioManager->playMusic("default");
    };
    refs.stopBgm = [this]() {
        if (audioManager) audioManager->stopBGM();
    };
    refs.playSe = [this]() {
        if (audioManager) audioManager->playSound("ui/click");
    };
    refs.stopAllSe = [this]() {
        if (audioManager) audioManager->stopAllSE();
    };
    refs.setMasterVolume = [this](float vol) {
        if (audioManager) audioManager->setMasterVolume(vol);
    };
    refs.muteAll = [this]() {
        if (audioManager) audioManager->setMasterVolume(0.0f);
    };
    refs.unmuteAll = [this]() {
        if (audioManager) audioManager->setMasterVolume(1.0f);
    };
    refs.listAudio = [this]() -> std::string {
        if (audioManager) return audioManager->getLoadedAudioList();
        return "Audio manager not available";
    };
    refs.getAudioStats = [this]() -> std::string {
        if (audioManager) return audioManager->getAudioStats();
        return "Audio stats not available";
    };
    // Phase 66: BGM browsing callbacks
    refs.listBgmTracks = [this]() -> std::string {
        if (!audioManager) return "Audio manager not available";
        std::string result = "=== BGM Tracks ===\n";
        const auto& defs = audioManager->getSoundDefs();
        int count = 0;
        for (const auto& pair : defs) {
            if (pair.second.type == 0) { // 0=BGM
                result += "  [" + std::to_string(count) + "] " + pair.first + "\n";
                result += "       File: " + pair.second.file + "\n";
                result += "       Vol: " + std::to_string(pair.second.volume) + "\n";
                count++;
            }
        }
        if (count == 0) {
            result += "  (No BGM tracks in sound definitions)\n";
            result += "  Use 'playbgm' to play default BGM\n";
        }
        result += "Total: " + std::to_string(count) + " tracks\n";
        return result;
    };
    refs.playBgmTrack = [this](const std::string& key) {
        if (audioManager) audioManager->playMusic(key);
    };
    refs.setBgmVolume = [this](float vol) {
        if (audioManager) audioManager->setBGMVolume(vol);
    };
    refs.getCurrentBgmInfo = [this]() -> std::string {
        if (!audioManager) return "Audio manager not available";
        return "Current BGM: playing\nVolume: " + std::to_string(audioManager->getBGMVolume());
    };

    // Asset callbacks
    refs.listTextures = [this]() -> std::string {
        if (assetManager) return assetManager->getLoadedTextureList();
        return "Asset manager not available";
    };
    refs.listModels = [this]() -> std::string {
        if (assetManager) return assetManager->getLoadedModelList();
        return "Asset manager not available";
    };
    refs.getTextureInfo = [this]() -> std::string {
        if (assetManager) return assetManager->getTextureCacheStats();
        return "Texture info not available";
    };
    refs.getModelInfo = [this]() -> std::string {
        if (assetManager) return assetManager->getModelCacheStats();
        return "Model info not available";
    };
    refs.getCacheStats = [this]() -> std::string {
        if (assetManager) return assetManager->getCacheStats();
        return "Cache stats not available";
    };
    refs.clearCache = [this]() {
        if (assetManager) assetManager->clearCache();
    };
    refs.reloadAssets = [this]() {
        if (assetManager) assetManager->reloadAllAssets();
    };
    refs.getMemoryUsage = [this]() -> std::string {
        if (assetManager) return assetManager->getMemoryUsage();
        return "Memory usage not available";
    };
    refs.getAssetStats = [this]() -> std::string {
        if (assetManager) return assetManager->getAssetStats();
        return "Asset stats not available";
    };
    // Phase 66: Texture browsing callbacks
    refs.listTexturesDetailed = [this]() -> std::string {
        if (!assetManager) return "Asset manager not available";
        return assetManager->getLoadedTextureList();
    };
    refs.getTextureDetail = [this](const std::string& name) -> std::string {
        if (!assetManager) return "Asset manager not available";
        return assetManager->getTextureCacheStats();
    };

    gameConsole->setGameSystemRefs(refs);
    LOGI("GameSystemRefs connected to game systems");

    // Initialize Settings UI
    LOGI("Creating SettingsUI...");
    settingsUI = std::make_unique<SettingsUI>();
    if (!settingsUI->initialize(textRenderer.get(), settingsManager.get(), this)) {
        LOGE("Failed to initialize SettingsUI");
        return false;
    }
    LOGI("SettingsUI initialized successfully");

    // Initialize SaveLoadUI (Phase 5+)
    LOGI("Creating SaveLoadUI...");
    saveLoadUI = std::make_unique<SaveLoadUI>();
    // Initialize SaveLoadUI after SaveManager is created
    // (will be fully initialized after SaveManager setup)
    LOGI("SaveLoadUI created (full initialization deferred)");

    // Initialize Asset Manager (before WorldManager)
    LOGI("Creating AssetManager...");
    assetManager = std::make_unique<AssetManager>();
    if (!assetManager->initialize()) {
        LOGE("Failed to initialize AssetManager");
        return false;
    }
    LOGI("AssetManager initialized successfully");

    // Load BSA archives (must succeed or game has no data)
    {
        LOGI("Loading BSA archives...");

        // Default BSA archive list for Oblivion
        const char* bsaArchives[] = {
            "Oblivion - Meshes.bsa",
            "Oblivion - Textures - Compressed.bsa",
            "Oblivion - Textures.bsa",
            "Oblivion - Sounds.bsa",
            "Oblivion - Voices.bsa",
            "Oblivion - Misc.bsa",
            "DLCShiveringIsles - Meshes.bsa",
            "DLCShiveringIsles - Textures - Compressed.bsa",
            "DLCShiveringIsles - Sounds.bsa",
            "DLCShiveringIsles - Voices.bsa",
            "DLCShiveringIsles - Misc.bsa"
        };

        int loadedCount = 0;
        for (const auto& bsa : bsaArchives) {
            if (assetManager->loadArchive(bsa)) {
                loadedCount++;
                LOGI("  [OK] Loaded BSA: %s", bsa);
            } else {
                LOGW("  [--] BSA not found (optional): %s", bsa);
            }
        }

        LOGI("Loaded %d / %zu BSA archives", loadedCount,
             sizeof(bsaArchives) / sizeof(bsaArchives[0]));
    }

    // Load ESM/ESP game data from BSA archives
    {
        LOGI("Loading ESM game data...");
        // Oblivion.esm is inside Oblivion - Misc.bsa
        if (assetManager->loadEsmFromArchive("Oblivion.esm")) {
            LOGI("  [OK] Loaded Oblivion.esm");
            LOGI("Record count: %zu", assetManager->getEsmManager().getRecordCount());
            LOGI("Plugin count: %zu", assetManager->getEsmManager().getPluginCount());

            // Log a sample of loaded records
            LOGI("CELL records: %zu", assetManager->getEsmManager().findRecordsByType("CELL"));
            LOGI("NPC_ records: %zu", assetManager->getEsmManager().findRecordsByType("NPC_"));
            LOGI("WEAP records: %zu", assetManager->getEsmManager().findRecordsByType("WEAP"));
        } else {
            LOGW("  [--] Oblivion.esm not found (will test without ESM data)");
        }
    }

    // Initialize NPC Manager (before WorldManager)
    LOGI("Creating NpcManager...");
    npcManager = std::make_unique<NpcManager>();
    if (!npcManager->initialize()) {
        LOGE("Failed to initialize NpcManager");
        return false;
    }
    LOGI("NpcManager initialized successfully");

    // Initialize World Manager
    LOGI("Creating WorldManager...");
    worldManager = std::make_unique<WorldManager>();
    LOGI("Calling WorldManager::initialize() with managers...");
    if (!worldManager->initialize(npcManager.get(), assetManager.get())) {
        LOGE("Failed to initialize WorldManager");
        return false;
    }
    LOGI("WorldManager initialized successfully");

    // Initialize Quest Manager
    questManager = std::make_unique<QuestManager>();
    if (!questManager->initialize(worldManager->getNpcManager())) {
        LOGE("Failed to initialize QuestManager");
        return false;
    }

    // Initialize Spell Manager (before CombatManager)
    spellManager = std::make_unique<SpellManager>();
    if (!spellManager->initialize(worldManager->getNpcManager())) {
        LOGE("Failed to initialize SpellManager");
        return false;
    }

    // Initialize NavMesh Manager
    navMeshManager = std::make_unique<oblivion::NavMeshManager>();

    // Initialize AI Scheduler (Phase 35: Radiant AI)
    aiScheduler = std::make_unique<ai::AIScheduler>();

    // Initialize Combat Manager (with SpellManager)
    combatManager = std::make_unique<CombatManager>();
    if (!combatManager->initialize(worldManager.get(), worldManager->getNpcManager(),
                                   spellManager.get())) {
        LOGE("Failed to initialize CombatManager");
        return false;
    }

    // Initialize PlayerController (Phase 3+)
    playerController = std::make_unique<PlayerController>();
    if (!playerController->initialize(worldManager.get())) {
        LOGE("Failed to initialize PlayerController");
        return false;
    }
    LOGI("PlayerController initialized successfully");

    // Phase 31: Initialize WorldLoader
    LOGI("Creating WorldLoader...");
    worldLoader = std::make_unique<WorldLoader>();
    worldLoader->init(assetManager.get(), nullptr);  // CollisionWorld will be set later if needed
    LOGI("WorldLoader initialized successfully");

    // Initialize InventoryManager (Phase 3+)
    inventoryManager = std::make_unique<InventoryManager>();
    if (!inventoryManager->initialize()) {
        LOGE("Failed to initialize InventoryManager");
        return false;
    }
    LOGI("InventoryManager initialized successfully");

    // Initialize InventoryUI (Phase 3+)
    inventoryUI = std::make_unique<InventoryUI>();
    if (!inventoryUI->initialize(inventoryManager->getPlayerInventory(), textRenderer.get())) {
        LOGE("Failed to initialize InventoryUI");
        return false;
    }
    LOGI("InventoryUI initialized successfully");

    // Initialize Performance Monitor
    performanceMonitor = std::make_unique<PerformanceMonitor>();
    performanceMonitor->initialize();
    LOGI("PerformanceMonitor initialized");

    // Initialize Save Manager
    saveManager = std::make_unique<SaveManager>();
    if (!saveManager->initialize()) {
        LOGE("Failed to initialize SaveManager");
    } else {
        LOGI("SaveManager initialized");
        // List available saves
        auto saves = saveManager->getSaveSlots();
        LOGI("Found %zu save slots", saves.size());
        // Register system pointers for binary save
        if (playerController) saveManager->setPlayerController(playerController.get());
        if (inventoryManager) saveManager->setInventoryManager(inventoryManager.get());
        if (spellManager) saveManager->setSpellManager(spellManager.get());
        if (questManager) saveManager->setQuestManager(questManager.get());
        if (worldManager) saveManager->setWorldManager(worldManager.get());
    }

    // Complete SaveLoadUI initialization (now that SaveManager is ready)
    if (saveLoadUI && saveManager) {
        if (!saveLoadUI->initialize(textRenderer.get(), saveManager.get(), this)) {
            LOGE("Failed to initialize SaveLoadUI");
        } else {
            LOGI("SaveLoadUI initialized successfully");
        }
    }

    // Initialize Audio Manager (Phase 8+)
#ifdef AUDIO_SYSTEM_ENABLED
    LOGI("Initializing AudioManager...");
    audioManager = std::make_unique<AudioManager>();
    if (!audioManager->initialize()) {
        LOGE("Failed to initialize AudioManager");
    } else {
        LOGI("AudioManager initialized successfully");
        audioManager->setListenerPosition(glm::vec3(0.0f, 1.7f, 0.0f));
        LOGD("Audio listener positioned at world center");
        
        // Load sound definitions from JSON
        LOGI("Loading sound definitions...");
        if (audioManager->loadSoundDefinitions("audio/sound_definitions.json")) {
            LOGI("Sound definitions loaded successfully");
        } else {
            LOGW("Failed to load sound definitions - audio will use manual clip loading");
        }
    }
#endif

    // Initialize Phase 36 Jolt Physics
    LOGI("Initializing Jolt Physics...");
    {
        auto& physics = oblivion::PhysicsManager::getInstance();
        if (physics.init()) {
            LOGI("Jolt Physics initialized successfully");
        } else {
            LOGE("Failed to initialize Jolt Physics");
        }
    }

    // Initialize Phase 9.1 Map System
    LOGI("Creating MapSystem...");
    mapSystem = std::make_unique<map::MapSystem>();
    mapSystem->setWorldBounds(-81920.0f, 81920.0f, -81920.0f, 81920.0f);
    LOGI("MapSystem initialized");

    // Create Map UI (fullscreen map)
    if (uiSystem) {
        auto fullMap = std::make_shared<ui::MapUI>("World Map");
        fullMap->setMapSystem(mapSystem.get());
        fullMap->setSize(static_cast<float>(screenWidth) * 0.8f, static_cast<float>(screenHeight) * 0.8f);
        fullMap->setPosition(static_cast<float>(screenWidth) * 0.1f, static_cast<float>(screenHeight) * 0.1f);
        fullMap->setVisible(false);
        fullMap->setDraggable(false); // Full-screen map doesn't need dragging

        // Apply background texture if available
        GLuint mapBgTex = TextureLoader::loadTextureFromAsset("textures/ui/main_background.png");
        if (mapBgTex != 0) {
            fullMap->setTexture(mapBgTex);
            fullMap->setTextureScaleMode(TextureScaleMode::STRETCH);
        }

        uiSystem->registerComponent(fullMap, 10);
        mapUI = fullMap.get();
        LOGI("MapUI created and registered in UISystem");

        // Create Mini-Map UI
        auto miniMap = std::make_shared<ui::MapUI>("MiniMap");
        miniMap->setMapSystem(mapSystem.get());
        miniMap->setMiniMapMode(true);
        miniMap->setSize(200.0f, 200.0f);
        miniMap->setPosition(static_cast<float>(screenWidth) - 220.0f, 20.0f);
        miniMap->setVisible(true);
        miniMap->onMiniMapTapped = [this]() {
            LOGI("Mini-map tapped - opening world map");
            toggleMap();
        };
        uiSystem->registerComponent(miniMap, 5);
        LOGI("MiniMap created and registered in UISystem");
    }

    // Initialize Phase 9.2 Inventory System
    LOGI("Creating InventoryGrid...");
    inventoryGrid = std::make_unique<inventory::InventoryGrid>(150.0f);
    LOGI("InventoryGrid initialized (max weight 150.0f)");

    equipmentManager = std::make_unique<inventory::EquipmentManager>();
    LOGI("EquipmentManager initialized");

    // Create test items
    {
        inventory::Item sword;
        sword.id = 1;
        sword.name = "Iron Sword";
        sword.description = "A basic iron sword.";
        sword.category = inventory::ItemCategory::Weapon;
        sword.rarity = inventory::ItemRarity::Common;
        sword.equipSlot = inventory::EquipSlot::Weapon;
        sword.weight = 3.5f;
        sword.value = 50;
        sword.stats.damage = 10;
        inventoryGrid->addItem(sword, 1);

        inventory::Item helm;
        helm.id = 2;
        helm.name = "Leather Helm";
        helm.category = inventory::ItemCategory::Armor;
        helm.equipSlot = inventory::EquipSlot::Head;
        helm.weight = 1.2f;
        helm.value = 30;
        helm.stats.defense = 5;
        inventoryGrid->addItem(helm, 1);

        inventory::Item potion;
        potion.id = 3;
        potion.name = "Health Potion";
        potion.category = inventory::ItemCategory::Consumable;
        potion.weight = 0.3f;
        potion.value = 15;
        potion.maxStack = 20;
        potion.healAmount = 25;
        inventoryGrid->addItem(potion, 5);

        inventory::Item questItem;
        questItem.id = 4;
        questItem.name = "Ancient Key";
        questItem.category = inventory::ItemCategory::Quest;
        questItem.weight = 0.1f;
        questItem.value = 0;
        inventoryGrid->addItem(questItem, 1);

        LOGI("Test items added to inventory");
    }

    // Create Inventory UI
    if (uiSystem) {
        auto invPanel = std::make_shared<ui::UIInventoryPanel>("Inventory");
        invPanel->setInventory(inventoryGrid.get());
        invPanel->setEquipment(equipmentManager.get());
        invPanel->setSize(static_cast<float>(screenWidth) * 0.85f, static_cast<float>(screenHeight) * 0.75f);
        invPanel->setPosition(static_cast<float>(screenWidth) * 0.075f, static_cast<float>(screenHeight) * 0.125f);
        invPanel->setVisible(false);
        uiSystem->registerComponent(invPanel, 15);
        uiInventoryPanel = invPanel.get();
        LOGI("UIInventoryPanel created and registered in UISystem");
    }

    // Initialize Launcher Screen (displayed before title screen)
    launcherScreen = std::make_unique<LauncherScreen>();
    launcherScreen->initialize(localizationManager.get(), textRenderer.get(),
                               settingsManager.get(), this);
    launcherScreen->setScreenSize(static_cast<int>(screenWidth), static_cast<int>(screenHeight));

    // Launcher callbacks
    launcherScreen->setOnPlayCallback([this]() {
        LOGI("Launcher Play clicked - transitioning to title screen");
        showLauncher = false;
        showTitleScreen = true;
        // Initialize title screen
        if (titleScreen) {
            titleScreen->initialize(localizationManager.get(), textRenderer.get());
            titleScreen->setScreenSize(static_cast<int>(screenWidth), static_cast<int>(screenHeight));
        }
    });

    launcherScreen->setOnExitCallback([this]() {
        LOGI("Launcher Exit clicked - requesting app exit");
        shouldExit = true;
    });

    LOGI("LauncherScreen initialized");

    // Initialize Title Screen
    titleScreen = std::make_unique<TitleScreen>();
    titleScreen->initialize(localizationManager.get(), textRenderer.get());

    // Initialize Quest UI
    questUI = std::make_unique<QuestUI>();
    questUI->initialize(questManager.get(), worldManager->getNpcManager(),
                        localizationManager.get());
    questUI->setTextRenderer(textRenderer.get());
    questUI->setScreenSize(screenWidth, screenHeight);

    LOGI("All game systems initialized");

    // Imperial Weave: initialize thin integration layer
        LOGI("Initializing Imperial Weave...");
        weave::ImperialWeave::instance().init(
            this,
            worldManager.get(),
            npcManager.get(),
            combatManager.get(),
            questManager.get(),
            nullptr,  // CollisionWorld - will be set when physics is integrated
            nullptr,  // AnimationPlayer - will be set when animation is integrated
            playerController.get(),
            inventoryManager.get(),
            spellManager.get(),
            audioManager.get(),
            &oblivion::PhysicsManager::getInstance()
        );
        imperialWeaveInitialized = true;

        // Connect CombatManager to Imperial Weave EventBus
        if (combatManager) {
            combatManager->setEventBus(&weave::ImperialWeave::instance().getEventBus());
            LOGI("CombatManager connected to Imperial Weave EventBus");
        }

        // Connect PlayerController to Imperial Weave EventBus for animation events
        if (playerController) {
            playerController->setEventBus(&weave::ImperialWeave::instance().getEventBus());
            playerController->subscribeToCombatEvents();
            LOGI("PlayerController connected to Imperial Weave EventBus");
        }

        // Connect WorldLoader to Imperial Weave EventBus for animation events
        if (worldLoader) {
            worldLoader->setEventBus(&weave::ImperialWeave::instance().getEventBus());
            LOGI("WorldLoader connected to Imperial Weave EventBus");
        }

        // Connect UIFloatingText to Imperial Weave EventBus for combat feedback
        if (floatingText) {
            auto& bus = weave::ImperialWeave::instance().getEventBus();
            bus.subscribe("COMBAT_ATTACK_HIT", [this](const weave::Event& e) {
                floatingText->addText("Hit!", screenWidth * 0.5f, screenHeight * 0.4f,
                                     UIFloatingText::DAMAGE, 1.5f);
            });
            bus.subscribe("COMBAT_CRITICAL_HIT", [this](const weave::Event& e) {
                floatingText->addText("CRITICAL!", screenWidth * 0.5f, screenHeight * 0.35f,
                                     UIFloatingText::CRITICAL, 2.0f);
            });
            bus.subscribe("COMBAT_BLOCK", [this](const weave::Event& e) {
                floatingText->addText("Blocked!", screenWidth * 0.5f, screenHeight * 0.4f,
                                     UIFloatingText::BLOCK, 1.5f);
            });
            bus.subscribe("COMBAT_PARRY", [this](const weave::Event& e) {
                floatingText->addText("Parry!", screenWidth * 0.5f, screenHeight * 0.4f,
                                     UIFloatingText::BUFF, 1.5f);
            });
            bus.subscribe("COMBAT_DODGE", [this](const weave::Event& e) {
                floatingText->addText("Dodge!", screenWidth * 0.5f, screenHeight * 0.4f,
                                     UIFloatingText::MISS, 1.5f);
            });
            LOGI("UIFloatingText connected to Imperial Weave EventBus");
        }

        // Connect AnimationSubscriber to Imperial Weave EventBus
        if (animSubscriber && worldLoader) {
            animSubscriber->init(&weave::ImperialWeave::instance().getEventBus(), worldLoader.get());
            animSubscriber->subscribeToEvents();
            LOGI("AnimationSubscriber connected to Imperial Weave EventBus");
        }

        // Connect AudioSubscriber to Imperial Weave EventBus
        if (audioSubscriber && audioManager) {
            audioSubscriber->init(&weave::ImperialWeave::instance().getEventBus(), audioManager.get());
            audioSubscriber->subscribeToEvents();

            // Connect NPC position callback for 3D spatial audio
            if (npcManager) {
                audioSubscriber->setNpcPositionCallback([this](uint32_t npcId) -> glm::vec3 {
                    auto npc = npcManager->getNPC(npcId);
                    if (npc) return npc->position;
                    return glm::vec3(0.0f, 0.0f, 0.0f);
                });
            }
            LOGI("AudioSubscriber connected to Imperial Weave EventBus");
        }

        LOGI("Imperial Weave initialized successfully");

    return true;
}

void Renderer::createTestScenario() {
    LOGI("=== createTestScenario() START ===");

    // Safety checks
    if (!worldManager) {
        LOGE("ERROR: worldManager is null in createTestScenario()");
        return;
    }

    if (!questManager) {
        LOGE("ERROR: questManager is null in createTestScenario()");
        return;
    }

    if (!combatManager) {
        LOGE("ERROR: combatManager is null in createTestScenario()");
        return;
    }

    if (!spellManager) {
        LOGE("ERROR: spellManager is null in createTestScenario()");
        return;
    }

    NpcManager* npcMgr = worldManager->getNpcManager();
    if (!npcMgr) {
        LOGE("ERROR: getNpcManager() returned null");
        return;
    }

    // Check if we have real ESM data loaded
    const auto& esmMgr = assetManager->getEsmManager();
    bool hasEsmData = (esmMgr.getPluginCount() > 0);

    if (hasEsmData) {
        LOGI("=== Building world from ESM data ===");

        // 1. Load CELL records into WorldManager
        const auto& esmCells = esmMgr.getAllCells();
        LOGI("Loading %zu cells from ESM data", esmCells.size());
        for (const auto& cell : esmCells) {
            LOGD("  Cell: 0x%08X '%s' (%s) grid=[%d,%d]",
                 cell.formID, cell.editorID.c_str(),
                 cell.fullName.c_str(), cell.gridX, cell.gridY);
            worldManager->addCellFromESM(
                cell.gridX, cell.gridY,
                cell.editorID, cell.fullName,
                cell.formID);
        }

        // 2. Build lookup: baseFormID → NPCData for reference resolution
        std::unordered_map<uint32_t, const oblivion::NPCData*> npcLookup;
        const auto& esmNpcs = esmMgr.getAllNPCs();
        for (const auto& npc : esmNpcs) {
            npcLookup[npc.formID] = &npc;
        }

        // 3. Process REFR references to place NPCs at correct positions
        const auto& refs = esmMgr.getAllReferences();
        LOGI("Processing %zu references from ESM data", refs.size());
        for (const auto& ref : refs) {
            auto it = npcLookup.find(ref.baseFormID);
            if (it != npcLookup.end()) {
                const oblivion::NPCData* npcData = it->second;
                auto npcPtr = npcMgr->createNPC(
                    npcData->fullName.empty() ? npcData->editorID : npcData->fullName,
                    ref.position);
                if (npcPtr) {
                    npcPtr->status.initialize(
                        static_cast<float>(npcData->health),
                        static_cast<float>(npcData->magicka),
                        npcData->level);
                    npcPtr->rotation = ref.rotation;
                    npcPtr->meshAssetPath = "meshes/characters/imperial_male.nif";
                    npcPtr->updateModelMatrix();

                    // Register ESM NPC with AI Scheduler (Phase 35: Radiant AI)
                    if (aiScheduler) {
                        aiScheduler->registerNPC(npcPtr->npcId);
                    }

                    LOGD("  Placed NPC: 0x%08X '%s' at (%.1f, %.1f, %.1f)",
                         ref.formID, npcData->fullName.c_str(),
                         ref.position.x, ref.position.y, ref.position.z);
                }
            }
        }

        // 4. Load LAND terrain data and assign to cells
        const auto& terrains = esmMgr.getAllTerrains();
        LOGI("Loading %zu terrain records from ESM data", terrains.size());
        for (const auto& terrain : terrains) {
            auto cell = worldManager->getCellByFormID(terrain.formID);
            if (cell && terrain.hasHeights()) {
                cell->heightData = terrain.heights;
                cell->isDirty = true;
                LOGD("  Assigned terrain to cell 0x%08X (%zu heights)",
                     terrain.formID, terrain.heights.size());
            }
        }

        // 5. Load WEAP records for reference
        const auto& weapons = esmMgr.getAllWeapons();
        LOGI("Loaded %zu weapons from ESM data", weapons.size());
        for (const auto& weapon : weapons) {
            LOGD("  Weapon: 0x%08X '%s' dmg=%u value=%u weight=%u",
                 weapon.formID, weapon.fullName.c_str(),
                 weapon.damage, weapon.value, weapon.weight);
        }

        // 6. Log worldspace definitions with bounds
        const auto& worlds = esmMgr.getAllWorlds();
        LOGI("Found %zu worldspaces", worlds.size());
        for (const auto& w : worlds) {
            LOGI("  WRLD: 0x%08X '%s' '%s' bounds=[%d,%d] to [%d,%d]",
                 w.formID, w.editorID.c_str(), w.fullName.c_str(),
                 w.minX, w.minY, w.maxX, w.maxY);
        }

                // 7. Import armor records from ESM into ItemFactory
                inventory::ItemFactory::getInstance().loadArmorsFromESM(esmMgr);

                // 8. Import spell records from ESM into SpellManager
                if (spellManager) {
                    spellManager->loadSpellsFromESM(esmMgr);
                }

                // 9. Process leveled lists (LVLI/LVLC/LVSP)
                const auto& leveledLists = esmMgr.getAllLeveledLists();
                LOGI("Found %zu leveled lists", leveledLists.size());
                size_t lvliCount = 0, lvlcCount = 0, lvspCount = 0;
                for (const auto& ll : leveledLists) {
                    // Count by type based on editorID prefix or entries
                    if (ll.editorID.find("LL") != std::string::npos) lvliCount++;
                    else if (ll.editorID.find("LC") != std::string::npos) lvlcCount++;
                    else lvspCount++;
                    LOGD("  LeveledList: 0x%08X '%s' chanceNone=%u flags=0x%02X entries=%zu",
                         ll.formID, ll.editorID.c_str(), ll.chanceNone, ll.flags, ll.entries.size());
                }
                LOGI("  LVLI=%zu LVLC=%zu LVSP=%zu", lvliCount, lvlcCount, lvspCount);

                // 10. Resolve LVLC lists to spawn creatures at player level 5 (test)
                uint32_t testPlayerLevel = 5;
                size_t spawnedFromLists = 0;
                for (const auto& ll : leveledLists) {
                    // Only process creature lists (those with NPC_ references)
                    if (ll.entries.empty()) continue;
                    auto resolved = esmMgr.resolveLeveledList(ll.formID, testPlayerLevel);
                    for (const auto& [refFormID, count] : resolved) {
                        const auto* npcData = esmMgr.findNPC(refFormID);
                        if (npcData) {
                            // Spawn creature from leveled list at origin
                            glm::vec3 spawnPos(spawnedFromLists * 3.0f, 0.0f, -10.0f);
                            auto npc = npcMgr->createNPC(
                                npcData->fullName.empty() ? npcData->editorID : npcData->fullName,
                                spawnPos);
                            if (npc) {
                                npc->status.initialize(
                                    static_cast<float>(npcData->health),
                                    static_cast<float>(npcData->magicka),
                                    npcData->level);
                                npc->meshAssetPath = "meshes/characters/imperial_male.nif";
                                npc->updateModelMatrix();

                                // Register spawned creature with AI Scheduler
                                if (aiScheduler) {
                                    aiScheduler->registerNPC(npc->npcId);
                                }

                                spawnedFromLists++;
                                LOGD("  Spawned from LVLC '%s': NPC '%s' (0x%08X) level=%u",
                                     ll.editorID.c_str(), npcData->fullName.c_str(),
                                     refFormID, npcData->level);
                            }
                        }
                    }
                }
                LOGI("Spawned %zu NPCs from leveled creature lists", spawnedFromLists);

                // 11. Load NavMesh data for AI pathfinding
                const auto& navMeshes = esmMgr.getAllNavMeshes();
                LOGI("Found %zu NavMesh records", navMeshes.size());
                size_t totalVertices = 0, totalTriangles = 0;
                for (const auto& nm : navMeshes) {
                    totalVertices += nm.vertices.size();
                    totalTriangles += nm.triangles.size();
                    LOGD("  NavMesh: 0x%08X '%s' verts=%zu tris=%zu",
                         nm.formID, nm.editorID.c_str(), nm.vertices.size(), nm.triangles.size());
                }
                LOGI("  Total: %zu vertices, %zu triangles", totalVertices, totalTriangles);

                // Load NavMesh data into NavMeshManager
                if (navMeshManager) {
                    navMeshManager->loadFromESM(esmMgr);
                }

                // Initialize AI Scheduler with game systems
                if (aiScheduler && npcManager && worldManager) {
                    aiScheduler->init(npcManager.get(), worldManager.get(), navMeshManager.get());
                    LOGI("AI Scheduler initialized with %zu registered NPCs", aiScheduler->getRegisteredNPCCount());
                }

                // 12. Log race and class data for character creation
                const auto& races = esmMgr.getAllRaces();
                LOGI("Found %zu races", races.size());
                for (const auto& race : races) {
                    LOGI("  RACE: 0x%08X '%s' '%s' HP=%u spells=%zu",
                         race.formID, race.editorID.c_str(), race.fullName.c_str(),
                         race.startingHealth, race.spellFormIDs.size());
                    LOGD("    STR=%u INT=%u WIL=%u AGI=%u SPD=%u END=%u PER=%u",
                         race.attrStrength, race.attrIntelligence, race.attrWillpower,
                         race.attrAgility, race.attrSpeed, race.attrEndurance, race.attrPersonality);
                }

                const auto& classes = esmMgr.getAllClasses();
                LOGI("Found %zu classes", classes.size());
                for (const auto& cls : classes) {
                    LOGI("  CLAS: 0x%08X '%s' '%s' spec=%u",
                         cls.formID, cls.editorID.c_str(), cls.fullName.c_str(), cls.specialization);
                }

                // 13. Log book data (skill books)
                const auto& books = esmMgr.getAllBooks();
                LOGI("Found %zu books", books.size());
                size_t skillBookCount = 0;
                for (const auto& book : books) {
                    if (book.teachesSkillID != 0) {
                        skillBookCount++;
                        LOGD("  SkillBook: 0x%08X '%s' skill=0x%08X level=%u value=%u",
                             book.formID, book.fullName.c_str(),
                             book.teachesSkillID, book.teachesSkillLevel, book.value);
                    }
                }
                LOGI("  Skill books: %zu", skillBookCount);

                // 14. Log remaining item types
                const auto& clothing = esmMgr.getAllClothing();
                LOGI("Found %zu clothing items", clothing.size());
                for (const auto& cl : clothing) {
                    LOGD("  CLOT: 0x%08X '%s' value=%u weight=%.1f",
                         cl.formID, cl.fullName.c_str(), cl.value, cl.weight);
                }

                const auto& ingredients = esmMgr.getAllIngredients();
                LOGI("Found %zu ingredients", ingredients.size());
                for (const auto& ing : ingredients) {
                    LOGD("  INGR: 0x%08X '%s' value=%u weight=%.1f",
                         ing.formID, ing.fullName.c_str(), ing.value, ing.weight);
                }

                const auto& alchemy = esmMgr.getAllAlchemy();
                LOGI("Found %zu alchemy items", alchemy.size());
                for (const auto& alc : alchemy) {
                    LOGD("  ALCH: 0x%08X '%s' value=%u weight=%.1f",
                         alc.formID, alc.fullName.c_str(), alc.value, alc.weight);
                }

                const auto& miscItems = esmMgr.getAllMiscItems();
                LOGI("Found %zu misc items", miscItems.size());
                for (const auto& misc : miscItems) {
                    LOGD("  MISC: 0x%08X '%s' value=%u weight=%.1f",
                         misc.formID, misc.fullName.c_str(), misc.value, misc.weight);
                }

                LOGI("ESM-based world generation complete");

                // Phase 31: Load NIF files as WorldEntities
                if (worldLoader) {
                    LOGI("=== Phase 31: Loading WorldEntities from NIF files ===");

                    // Test NIF paths (common Oblivion meshes)
                    const char* testNifs[] = {
                        "meshes/characters/imperial_male.nif",
                        "meshes/creatures/imp.nif",
                        "meshes/architecture/buildings/imperial_house_01.nif",
                        "meshes/furniture/chair_01.nif",
                        "meshes/clutter/barrel_01.nif"
                    };

                    for (size_t i = 0; i < sizeof(testNifs)/sizeof(testNifs[0]); ++i) {
                        glm::vec3 pos(static_cast<float>(i) * 5.0f, 0.0f, -15.0f);
                        WorldEntity entity;

                        // Try loading as actor first (has skeleton/animation), then static
                        entity = worldLoader->loadActor(testNifs[i], pos);
                        if (entity.mesh || entity.skinnedMesh) {
                            entity.entityId = worldLoader->getNextEntityId();
                            worldEntities.push_back(std::move(entity));
                            LOGI("  [OK] Loaded actor: %s (id=%u)", testNifs[i], worldEntities.back().entityId);
                        } else {
                            entity = worldLoader->loadStatic(testNifs[i], pos);
                            if (entity.mesh || entity.skinnedMesh) {
                                entity.entityId = worldLoader->getNextEntityId();
                                worldEntities.push_back(std::move(entity));
                                LOGI("  [OK] Loaded static: %s (id=%u)", testNifs[i], worldEntities.back().entityId);
                            } else {
                                LOGW("  [--] Failed to load: %s", testNifs[i]);
                            }
                        }
                    }

                    // Wire player skeleton/animation if we loaded an actor entity
                    for (auto& ent : worldEntities) {
                        if (ent.type == WorldEntityType::ACTOR && ent.skeleton && ent.animator) {
                            playerController->setSkeleton(ent.skeleton.get());
                            playerController->setAnimator(ent.animator.get());
                            LOGI("  PlayerController wired to entity %u skeleton/animation", ent.entityId);
                            break;  // Use first actor for player
                        }
                    }

                    LOGI("Phase 31: Loaded %zu WorldEntities (cache size=%zu)",
                         worldEntities.size(), worldLoader->getCacheSize());
                }
    } else {
        LOGI("=== No ESM data available, using hardcoded test scenario ===");
        // Fall back to hardcoded test (existing code below)
    }

    // Declare spell variables at function scope so they're available for spell casting
    uint32_t fireball = 0;
    uint32_t heal = 0;
    uint32_t restoreMana = 0;

    // Create test NPCs (always create at least basic test NPCs)
    NpcManager* npcMgr2 = npcMgr;  // reuse pointer
    
    // Create NPCs using available data
    // Place enemy (Izar) in front of player, ally (Hellas) to the side
    auto izar = npcMgr2->createNPC("Izar", glm::vec3(10.0f, 0.0f, -5.0f));
    auto hellas = npcMgr2->createNPC("Hellas", glm::vec3(-5.0f, 0.0f, 0.0f));

    if (!izar) { LOGE("ERROR: Failed to create NPC 'Izar'"); return; }
    if (!hellas) { LOGE("ERROR: Failed to create NPC 'Hellas'"); return; }

    if (izar && hellas) {
        izar->status.initialize(150.0f, 100.0f, 5);
        hellas->status.initialize(120.0f, 80.0f, 4);

        // Register NPCs with AI Scheduler (Phase 35: Radiant AI)
        if (aiScheduler) {
            aiScheduler->registerNPC(izar->npcId);
            aiScheduler->registerNPC(hellas->npcId);
            LOGI("Registered test NPCs with AI Scheduler: Izar=%u, Hellas=%u",
                 izar->npcId, hellas->npcId);
        }

        // Set mesh asset paths (from Oblivion ISO extracted meshes)
        // These are relative paths that will be resolved by AssetManager
        izar->meshAssetPath = "meshes/creatures/imp.nif";  // Monster model
        hellas->meshAssetPath = "meshes/characters/imperial_male.nif";  // NPC model
        LOGI("NPC mesh paths set: Izar=%s, Hellas=%s",
             izar->meshAssetPath.c_str(), hellas->meshAssetPath.c_str());

        // Create test quests
        uint32_t quest1 = questManager->createQuest(izar->npcId, "Kill the Monster",
                                                    "Slay the beast terrorizing the area");
        uint32_t quest2 = questManager->createQuest(hellas->npcId, "Collect Items",
                                                    "Gather 5 crystals for the mage");

        if (quest1 != 0) {
            questManager->addObjective(quest1, "Defeat the monster", 1);
            QuestReward reward1;
            reward1.goldAmount = 100;
            reward1.experiencePoints = 150.0f;
            questManager->setQuestReward(quest1, reward1);
            izar->addQuestToOffer(quest1);
        }

        if (quest2 != 0) {
            questManager->addObjective(quest2, "Find crystals", 5);
            QuestReward reward2;
            reward2.goldAmount = 75;
            reward2.experiencePoints = 100.0f;
            questManager->setQuestReward(quest2, reward2);
            hellas->addQuestToOffer(quest2);
        }

        LOGI("Test quests created: Quest1=%u from Izar, Quest2=%u from Hellas",
             quest1, quest2);

        // Create test spells
        if (spellManager) {
            if (hasEsmData) {
                // ESM mode: pick the first Destruction and Restoration spells
                const auto& spells = esmMgr.getAllSpells();
                for (const auto& s : spells) {
                    uint32_t spellId = s.formID;
                    if (spellId == 0) continue;

                    // Assign first Destruction spell to Izar (monster)
                    if (s.effectType == 2 && fireball == 0) {
                        fireball = spellId;
                        spellManager->teachSpellToNpc(izar->npcId, spellId);
                        spellManager->equipSpellToNpc(izar->npcId, spellId);
                        LOGI("  ESM Destruction spell for Izar: 0x%08X '%s'", spellId, s.fullName.c_str());
                    }

                    // Assign first Restoration spell to Hellas (healer)
                    if (s.effectType == 5 && heal == 0) {
                        heal = spellId;
                        spellManager->teachSpellToNpc(hellas->npcId, spellId);
                        spellManager->equipSpellToNpc(hellas->npcId, spellId);
                        LOGI("  ESM Restoration spell for Hellas: 0x%08X '%s'", spellId, s.fullName.c_str());
                    }

                    // Assign first Mysticism spell to both
                    if (s.effectType == 4 && restoreMana == 0) {
                        restoreMana = spellId;
                        spellManager->teachSpellToNpc(izar->npcId, spellId);
                        spellManager->equipSpellToNpc(izar->npcId, spellId);
                        LOGI("  ESM Mysticism spell for both: 0x%08X '%s'", spellId, s.fullName.c_str());
                    }

                    // Also teach the Restoration spell to Izar so he can heal too
                    if (heal != 0 && spellId == heal && izar) {
                        spellManager->teachSpellToNpc(izar->npcId, heal);
                        spellManager->equipSpellToNpc(izar->npcId, heal);
                    }
                }
                LOGI("ESM spells assigned: fireball=0x%08X, heal=0x%08X, restoreMana=0x%08X",
                     fireball, heal, restoreMana);
            } else {
                // No ESM: use hardcoded spells
                // Destruction magic: Fireball
                fireball = spellManager->createSpell(
                    "Fireball", "Fireball",
                    MagicSchool::DESTRUCTION, 50.0f, 30.0f);
                if (fireball != 0) {
                    spellManager->addEffectToSpell(fireball,
                        SpellEffect(SpellEffectType::DAMAGE, 30.0f, 0.0f));
                    spellManager->teachSpellToNpc(izar->npcId, fireball);
                    spellManager->equipSpellToNpc(izar->npcId, fireball);
                }

                // Restoration magic: Heal
                heal = spellManager->createSpell(
                    "Heal", "Heal",
                    MagicSchool::RESTORATION, 40.0f, 0.0f);
                if (heal != 0) {
                    spellManager->addEffectToSpell(heal,
                        SpellEffect(SpellEffectType::HEAL, 50.0f, 0.0f));
                    spellManager->teachSpellToNpc(hellas->npcId, heal);
                    spellManager->teachSpellToNpc(izar->npcId, heal);
                    spellManager->equipSpellToNpc(hellas->npcId, heal);
                    spellManager->equipSpellToNpc(izar->npcId, heal);
                }

                // Mysticism magic: Restore Mana
                restoreMana = spellManager->createSpell(
                    "Restore Mana", "Restore Mana",
                    MagicSchool::MYSTICISM, 30.0f, 0.0f);
                if (restoreMana != 0) {
                    spellManager->addEffectToSpell(restoreMana,
                        SpellEffect(SpellEffectType::RESTORE_MANA, 40.0f, 0.0f));
                    spellManager->teachSpellToNpc(izar->npcId, restoreMana);
                    spellManager->equipSpellToNpc(izar->npcId, restoreMana);
                }

                LOGI("Test spells created: Fireball=%u, Heal=%u, RestoreMana=%u",
                     fireball, heal, restoreMana);
            }
        }

        // Teach spells to the PLAYER directly (Player is not in NpcManager)
        if (playerController && playerController->getPlayer()) {
            auto& player = *playerController->getPlayer();
            LOGI("Player ID=%u, knownSpells.size()=%zu, equippedSpells.size()=%zu BEFORE teaching",
                 player.playerId, player.knownSpells.size(), player.equippedSpells.size());
            if (fireball != 0) {
                player.knownSpells.push_back(fireball);
                player.equippedSpells.push_back(fireball);
                LOGI("Taught Fireball to player (direct), spellId=%u", fireball);
            }
            if (heal != 0) {
                player.knownSpells.push_back(heal);
                player.equippedSpells.push_back(heal);
                LOGI("Taught Heal to player (direct), spellId=%u", heal);
            }
            if (restoreMana != 0) {
                player.knownSpells.push_back(restoreMana);
                LOGI("Taught RestoreMana to player (direct), spellId=%u", restoreMana);
            }
            LOGI("Player knownSpells.size()=%zu, equippedSpells.size()=%zu AFTER teaching",
                 player.knownSpells.size(), player.equippedSpells.size());
        }

        // Initiate test combat
        if (combatManager) {
            combatManager->initiateCombat(izar, hellas);
            LOGI("Combat initiated: Izar vs Hellas");
        }

        // Test spell casting - FIX: Use correct spell IDs instead of hardcoded values
        LOGI("Testing spell casting...");
        if (fireball != 0) {
            LOGI("Casting Fireball (ID=%u) from Izar to Hellas", fireball);
            spellManager->castSpell(izar->npcId, fireball, hellas->npcId);
        }
        if (heal != 0) {
            LOGI("Casting Heal (ID=%u) from Hellas to self", heal);
            spellManager->castSpell(hellas->npcId, heal, hellas->npcId);
        }
        if (restoreMana != 0) {
            LOGI("Casting Restore Mana (ID=%u) from Izar to self", restoreMana);
            spellManager->castSpell(izar->npcId, restoreMana, izar->npcId);
        }
    }

    npcMgr->logNpcStatus();
    questManager->logQuestStatus();
    combatManager->logCombatStatus();
    if (spellManager) {
        spellManager->logSpellStatus();
    }
}

void Renderer::render(float deltaTime) {
    // Launcher takes priority - render and return early
    if (showLauncher && launcherScreen) {
        launcherScreen->update(deltaTime);
        launcherScreen->render();

        if (launcherScreen->isTransitioning()) {
            // Wait for fade completion
        }
        if (performanceMonitor) performanceMonitor->endFrame();
        return;
    }

    // Set joystick input before Imperial Weave update
    if (playerController && joystick) {
        glm::vec2 input = joystick->getInputValue();
        playerController->setJoystickInput(input.x, input.y);
    }

    // Update audio listener position before Imperial Weave update
#ifdef AUDIO_SYSTEM_ENABLED
    if (audioManager && worldManager) {
        const glm::vec3& cameraPos = worldManager->getCameraPosition();
        const glm::vec3& cameraForward = worldManager->getCameraForward();
        const glm::vec3& cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
        audioManager->setListenerPosition(cameraPos);
        audioManager->setListenerOrientation(cameraForward, cameraUp);

        // Update AudioSubscriber player position for 3D combat sounds
        if (audioSubscriber) {
            audioSubscriber->setPlayerPosition(cameraPos);
        }
    }
#endif

    // Imperial Weave: process game logic updates (events, world, AI, animation, physics)
    if (imperialWeaveInitialized) {
        weave::ImperialWeave::instance().update(deltaTime);
    }

    // Phase 35: Radiant AI — update AI scheduler after ImperialWeave
    if (aiScheduler) {
        aiScheduler->update(deltaTime);
    }

    // Begin performance monitoring
    if (performanceMonitor) {
        performanceMonitor->beginFrame();
    }

    // Update Title Screen
    if (showTitleScreen) {
        // BUG FIX: Null check titleScreen before access - it may not be created if init() failed early
        if (!titleScreen) {
            LOGE("titleScreen is null but showTitleScreen is true - skipping render");
            if (performanceMonitor) {
                performanceMonitor->endFrame();
            }
            return;
        }
        titleScreen->update(deltaTime);
        titleScreen->render();

        // Check if Load Game was requested (Phase 5+)
        if (titleScreen->isLoadGameRequested()) {
            titleScreen->resetLoadGameRequest();
            if (saveLoadUI) {
                saveLoadUI->open(SaveLoadUI::Mode::LOAD);  // Open load UI
                LOGI("SaveLoadUI opened in LOAD mode from title screen menu");
            }
        }

        // Check if Settings was requested
        if (titleScreen->isSettingsRequested()) {
            titleScreen->resetSettingsRequest();
            if (settingsUI) {
                settingsUI->toggle();  // Open settings UI
                LOGI("Settings UI opened from title screen menu");
            }
        }

        // Check if Credits was requested
        if (titleScreen->isCreditsRequested()) {
            titleScreen->resetCreditsRequest();
            // TODO: Implement credits screen overlay
            LOGI("Credits requested from title screen (not yet implemented)");
        }

        // Check if Quit was requested (return to launcher)
        if (titleScreen->isQuitRequested()) {
            titleScreen->resetQuitRequest();
            showTitleScreen = false;
            showLauncher = true;
            if (launcherScreen) {
                launcherScreen->initialize(localizationManager.get(), textRenderer.get());
                launcherScreen->setScreenSize(static_cast<int>(screenWidth), static_cast<int>(screenHeight));
            }
            LOGI("Title screen Quit - returning to launcher");
        }

        if (titleScreen->isGameStarted()) {
            showTitleScreen = false;
            // Show combat buttons when game starts
            if (joystick) joystick->setVisible(true);
            if (attackButton) { attackButton->setVisible(true); LOGD("ATK button set visible"); }
            if (blockButton) { blockButton->setVisible(true); LOGD("BLK button set visible"); }
            if (castSpellButton) { castSpellButton->setVisible(true); LOGD("MAG button set visible"); }
            for (auto& btn : quickSlotButtons) {
                if (btn) btn->setVisible(true);
            }
            LOGI("Title screen closed, starting main game");
        }
        // Skip frame rate control for quick return
        if (performanceMonitor) {
            performanceMonitor->endFrame();
        }
        return;  // Skip game rendering while title screen is active
    }

    // Update Phase 9 UI Framework
    if (uiSystem) {
        uiSystem->update(deltaTime);
    }

    // Update Floating Combat Text
    if (floatingText) {
        floatingText->update(deltaTime);
    }

    // Update Animation Subscriber
    if (animSubscriber) {
        animSubscriber->updateNpcAnimations(deltaTime);
    }

    // Update map system with player position and cell discovery
    if (mapSystem && worldManager) {
        glm::vec3 playerPos3D = worldManager->getPlayerPosition();
        glm::vec2 playerPos2D(playerPos3D.x, playerPos3D.z);
        mapSystem->setPlayerPosition(playerPos2D);

        // Auto-discover cells around player
        float cellSize = mapSystem->getCellSize();
        glm::vec2 cellCoord = map::MapSystem::worldToCell(playerPos2D, cellSize);
        int pcx = static_cast<int>(cellCoord.x);
        int pcy = static_cast<int>(cellCoord.y);

        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                int cx = pcx + dx;
                int cy = pcy + dy;
                if (!mapSystem->isCellDiscovered(cx, cy)) {
                    mapSystem->discoverCell(cx, cy);

                    // Set procedural cell info with terrain color
                    map::CellInfo info;
                    info.x = cx;
                    info.y = cy;
                    info.discovered = true;

                    // Procedural terrain color based on coordinates
                    uint32_t hash = static_cast<uint32_t>(cx * 374761393u + cy * 668265263u);
                    int terrainType = hash % 10;
                    if (terrainType < 5) {
                        info.terrainColor = 0xFF4A8C4A; // Grass green
                        info.name = "Wilderness";
                    } else if (terrainType < 7) {
                        info.terrainColor = 0xFF8C8C4A; // Hills brown-yellow
                        info.name = "Hills";
                    } else if (terrainType < 8) {
                        info.terrainColor = 0xFF4A6A8C; // Water blue
                        info.name = "Lake";
                    } else {
                        info.terrainColor = 0xFF7A7A7A; // Mountains gray
                        info.name = "Mountains";
                    }
                    mapSystem->setCellInfo(cx, cy, info);
                }
            }
        }

        // Update quest markers from active quests
        if (questManager && npcManager) {
            auto activeQuests = questManager->getActiveQuests();
            // Clear old quest markers
            mapSystem->clearMarkersByType(map::MarkerType::QuestMain);
            mapSystem->clearMarkersByType(map::MarkerType::QuestSide);
            // Add markers for active quests at giver NPC positions
            for (const auto& quest : activeQuests) {
                if (!quest) continue;
                auto npc = npcManager->getNPC(quest->giverNpcId);
                if (!npc) continue;
                map::MapMarker marker;
                marker.type = map::MarkerType::QuestSide;
                marker.worldPos = glm::vec2(npc->position.x, npc->position.z);
                marker.label = quest->title;
                marker.questId = quest->questId;
                marker.color = 0xFFFFD700; // Gold color ABGR
                mapSystem->addMarker(marker);
            }
        }
    }

    // Update game systems
    // NOTE: worldManager->update() is now handled by ImperialWeave (phaseWorldUpdate)
    // if (worldManager) {
    //     worldManager->update(deltaTime);
    // }

    // NOTE: playerController->update() is now handled by ImperialWeave (phasePlayerUpdate)
    // Joystick input is set before update - this needs to be moved to playerController's update method
    // if (playerController) {
    //     if (joystick) {
    //         glm::vec2 input = joystick->getInputValue();
    //         playerController->setJoystickInput(input.x, input.y);
    //     }
    //     playerController->update(deltaTime);
    // }

    // NOTE: inventoryManager->update() is now handled by ImperialWeave (phaseInventoryUpdate)
    // if (inventoryManager) {
    //     inventoryManager->update(deltaTime);
    // }

    // NOTE: questManager->update() is now handled by ImperialWeave (phaseQuestUpdate)
        // if (questManager) {
        //     questManager->update(deltaTime);
        // }

    // NOTE: combatManager->update() is now handled by ImperialWeave (phaseCombatUpdate)
        // if (combatManager) {
        //     combatManager->update(deltaTime);
        // }

    // NOTE: spellManager->update() is now handled by ImperialWeave (phaseSpellUpdate)
    // if (spellManager) {
    //     spellManager->update(deltaTime);
    // }

    // NOTE: audioManager->update() is now handled by ImperialWeave (phaseAudioUpdate)
    // #ifdef AUDIO_SYSTEM_ENABLED
    // if (audioManager && worldManager) {
    //     // Get camera position from world manager and set as audio listener position
    //     const glm::vec3& cameraPos = worldManager->getCameraPosition();
    //     const glm::vec3& cameraForward = worldManager->getCameraForward();
    //     const glm::vec3& cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);  // Standard up vector
    //
    //     audioManager->setListenerPosition(cameraPos);
    //     audioManager->setListenerOrientation(cameraForward, cameraUp);
    //     audioManager->update(deltaTime);
    // }
    // #endif

    // ===== RETRO FILTER: Bind scene framebuffer for rendering =====
    if (retroFilter) {
        retroFilter->bindSceneFramebuffer();
    }

    // Render World (main game scene) - Clear with game background color
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);  // Dark gray for game screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable depth testing for proper face rendering
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Load and bind NPC meshes via AssetManager
    if (assetManager && worldManager) {
        NpcManager* npcMgr = worldManager->getNpcManager();
        if (npcMgr) {
            auto allNpcs = npcMgr->getAllNPCs();
            for (const auto& npc : allNpcs) {
                if (npc) {
                    // Load mesh if not already loaded and asset path is set
                    if (!npc->mesh && !npc->meshAssetPath.empty()) {
                        npc->mesh = assetManager->loadNifMesh(npc->meshAssetPath);
                        if (npc->mesh) {
                            LOGD("Loaded mesh for NPC %u: %s", npc->npcId, npc->meshAssetPath.c_str());
                        } else {
                            LOGW("Failed to load mesh for NPC %u: %s", npc->npcId, npc->meshAssetPath.c_str());
                        }
                    }

                    // Update model matrix every frame
                    npc->updateModelMatrix();
                }
            }
        }
    }

    // Safety check: if initialization failed, don't try to render
    if (!initialized) {
        static int nullRenderCount = 0;
        if (nullRenderCount % 60 == 0) {  // Log every 60 frames (~1 second at 60 FPS)
            LOGE("CRITICAL: render() called but Renderer is not initialized! worldManager=%p",
                 worldManager.get());
        }
        nullRenderCount++;
        // Just clear the screen and return to prevent crash
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }

    // Render world objects
    if (worldManager) {
        LOGD("Calling worldManager->render()");
        worldManager->render();
        LOGD("worldManager->render() completed");
    } else {
        LOGW("worldManager is null!");
    }

    // Phase 31: Render WorldEntities (skinned meshes with bone matrices)
    if (!worldEntities.empty()) {
        // Create skinning shader if not exists (static for reuse)
        static ShaderProgram skinningShader;
        static bool shaderInitialized = false;
        if (!shaderInitialized) {
            skinningShader.compile(SkinningShader::vertexSource, SkinningShader::fragmentSource);
            shaderInitialized = true;
        }

        for (auto& entity : worldEntities) {
            if (!entity.isActive || !entity.isVisible) continue;

            // Update skeleton animation if entity has one
            if (entity.skeleton && entity.animator) {
                // Animation is already updated by PlayerController for player entity
                // For other entities, update here
                if (entity.type != WorldEntityType::ACTOR || !playerController) {
                    entity.animator->update(deltaTime);
                    entity.skeleton->update();
                }
            }

            // Compute model matrix
            glm::mat4 modelMatrix = entity.getModelMatrix();

            // Render skinned mesh
            if (entity.skinnedMesh && entity.skeleton) {
                const auto& boneMatrices = entity.skeleton->getSkinningMatrices();
                if (!boneMatrices.empty()) {
                    entity.skinnedMesh->updateBoneMatrices(boneMatrices);
                    entity.skinnedMesh->render(skinningShader, modelMatrix);
                }
            }
            // Render static mesh
            else if (entity.mesh) {
                entity.mesh->render(skinningShader, modelMatrix);
            }
        }
    }

    // ===== RETRO FILTER: Apply post-processing effects and render to screen =====
    if (retroFilter) {
        retroFilter->apply(retroSettings);
        retroFilter->renderToScreen();
    } else {
        LOGE("CRITICAL: retroFilter is NULL!");
    }

    // ===== NATIVE UI & HUD: Render directly on top of the screen at crisp, 100% full native resolution =====
    // Render 2D UI and HUD AFTER applying retro filter to completely prevent text corruption and layout distortion.

    // Update debug systems
    if (gameConsole) {
        gameConsole->update(deltaTime);
    }

    // Render UI
    if (questUI) {
        questUI->render();
    }

    // Render Inventory UI
    if (inventoryUI && inventoryUI->isVisible()) {
        inventoryUI->render();
    }

    // Render Debug systems
    if (gameConsole) {
        gameConsole->render();
    }

    // Render SaveLoadUI if visible (higher priority than SettingsUI)
    if (saveLoadUI && saveLoadUI->isVisible()) {
        saveLoadUI->render();
    }

    // Render Settings UI if visible
    if (settingsUI && settingsUI->isVisible()) {
        settingsUI->render();
    }

    // Render Phase 9 UI Framework components (overlays on top of existing UI)
    if (uiSystem) {
        uiSystem->render();
    }

    // Render Floating Combat Text
    if (floatingText) {
        floatingText->render();
    }

    // Frame rate control - enforce target FPS
    // Note: native_activity.cpp also has frame timing, but this provides more precise control
    auto currentFrameTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> frameElapsed = currentFrameTime - lastFrameTime;
    float elapsedMs = frameElapsed.count();

    if (elapsedMs < frameTimeThreshold) {
        // Sleep to maintain target FPS (with microsecond precision)
        float sleepTimeMs = frameTimeThreshold - elapsedMs;
        auto sleepDuration = std::chrono::microseconds(static_cast<long long>(sleepTimeMs * 1000.0f));
        std::this_thread::sleep_for(sleepDuration);
        lastFrameTime = std::chrono::high_resolution_clock::now();
    } else {
        lastFrameTime = currentFrameTime;
    }

    // End performance monitoring (after frame limiting)
    if (performanceMonitor) {
        performanceMonitor->endFrame();

        // Log performance metrics every 300 frames (5 seconds at 60fps)
        static int frameCounter = 0;
        frameCounter++;
        if (frameCounter >= 300) {
            performanceMonitor->logPerformanceReport();
            frameCounter = 0;
        }
    }

    LOGD("Frame rendered: deltaTime=%.3f, FPS=%.1f, Target FPS=%d",
         deltaTime, performanceMonitor ? performanceMonitor->getFPS() : 0.0f, targetFPS);
}

void Renderer::onTouchEvent(int pointerId, float x, float y, int action) {
    LOGD("=== Touch event detected === ID: %d, Action: %d, Coords: (%.1f, %.1f)", pointerId, action, x, y);

    // DebugSystem gesture detection (3-finger tap -> menu, 2-finger double-tap -> HUD)
    int pointerCount = static_cast<int>(touchStates.size()) + 1;
    DebugSystem::getInstance().onTouch(action, pointerCount, x, y,
                                       static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch()).count()));

    float dx = 0.0f;
    float dy = 0.0f;

    if (action == 0 || action == 5) { // DOWN
        touchStates[pointerId] = {x, y, true};
    } else if (action == 2) { // MOVE
        auto it = touchStates.find(pointerId);
        if (it != touchStates.end() && it->second.active) {
            dx = x - it->second.lastX;
            dy = y - it->second.lastY;
            it->second.lastX = x;
            it->second.lastY = y;
        }
    } else if (action == 1 || action == 6 || action == 3) { // UP or CANCEL
        // Erase entry to prevent unbounded map growth (memory leak fix)
        touchStates.erase(pointerId);
    }

    // Phase 9: UISystem handles all actions
    if (uiSystem) {
        bool uiHandled = false;
        if (action == 0 || action == 5) {
            uiHandled = uiSystem->onTouchDown(x, y, pointerId);
        } else if (action == 1 || action == 6) {
            uiHandled = uiSystem->onTouchUp(x, y, pointerId);
        } else if (action == 2) {
            uiHandled = uiSystem->onTouchMove(x, y, dx, dy, pointerId);
        }
        
        if (uiHandled) {
            LOGD("Touch consumed by UISystem");
            return;
        }
        LOGD("UISystem did not handle touch");
    }

    // GameConsole handles touch when visible
    if (gameConsole && gameConsole->isVisible()) {
        gameConsole->onTouchEvent(x, y, action);
        return;
    }

    // Only process legacy UI elements on ACTION_DOWN (0 or 5)
    if (action == 0 || action == 5) {
        if (saveLoadUI && saveLoadUI->isVisible()) {
            saveLoadUI->onTouchEvent(x, y);
            if (saveLoadUI->shouldReturnToMenu()) {
                saveLoadUI->resetReturnFlag();
                saveLoadUI->close();
            }
            return;
        }

        if (settingsUI && settingsUI->isVisible()) {
            settingsUI->onTouchEvent(x, y);
            if (settingsUI->shouldReturnToMenu()) {
                settingsUI->resetReturnFlag();
                settingsUI->toggle();
            }
            return;
        }

        if (inventoryUI && inventoryUI->isVisible()) {
            inventoryUI->onTouchEvent(x, y);
            return;
        }

        if (!showTitleScreen && worldManager && questUI) {
            questUI->onTouchEvent(x, y);
        }
    }

    // Launcher handles touch when active
    if (showLauncher && launcherScreen) {
        launcherScreen->onTouchEvent(x, y, action);
        return;
    }

    // TitleScreen handles all touch actions (DOWN, UP, MOVE) for button click callbacks
    if (showTitleScreen && titleScreen) {
        LOGD("Touch dispatched to TitleScreen at (%.1f, %.1f), action=%d", x, y, action);
        titleScreen->onTouchEvent(x, y, action);
        return;
    }

    // In-game camera control - only on MOVE (action 2) and if not clicking a UI
    if (!showTitleScreen && worldManager) {
        if (action == 2 && playerController) {
            // Check if touch is on right side of screen (for camera rotation)
            // Hardcoding a check assuming half screen width is around 1000px, 
            // but normally we should check real screen size. Let's just pass dx/dy for now
            // since Joystick will consume touches on the left side via UISystem.
            playerController->onTouchInput(dx, dy);
        }
    }
}
void Renderer::cleanup() {
    LOGI("Renderer cleaning up");

    // Imperial Weave: shutdown integration layer
    if (imperialWeaveInitialized) {
        weave::ImperialWeave::instance().shutdown();
        imperialWeaveInitialized = false;
        LOGI("Imperial Weave shut down");
    }

    // Phase 36: Shutdown Jolt Physics
    oblivion::PhysicsManager::getInstance().shutdown();
    LOGI("Jolt Physics shut down");

    // Clean up static UI drawing programs/buffers to prevent stale GL context handles across EGL context recreations
    UIDrawHelper::cleanup();

    if (mapSystem) {
        mapSystem.reset();
    }

    if (equipmentManager) {
        equipmentManager.reset();
    }

    if (inventoryGrid) {
        inventoryGrid.reset();
    }

    if (uiSystem) {
        uiSystem->cleanup();
        uiSystem = nullptr;
    }

    if (retroFilter) {
        retroFilter->cleanup();
        retroFilter = nullptr;
    }

    if (performanceMonitor) {
        performanceMonitor->logDetailedMetrics();
    }

    if (gameConsole) {
        gameConsole->cleanup();
    }

    DebugSystem::getInstance().cleanup();

    if (settingsUI) {
        settingsUI->cleanup();
    }

    if (saveLoadUI) {
        saveLoadUI->cleanup();
    }

    if (inventoryUI) {
        inventoryUI = nullptr;
    }

    if (textRenderer) {
        textRenderer->cleanup();
    }

    if (questUI) {
        questUI->cleanup();
    }

    if (questManager) {
        questManager->cleanup();
    }

    if (combatManager) {
        combatManager->cleanup();
    }

    if (spellManager) {
        spellManager->cleanup();
    }

    if (playerController) {
        playerController->cleanup();
        playerController = nullptr;
    }

    if (inventoryManager) {
        inventoryManager->cleanup();
        inventoryManager = nullptr;
    }

    if (assetManager) {
        assetManager->cleanup();
    }

    if (worldManager) {
        worldManager->cleanup();
    }

    if (localizationManager) {
        localizationManager->cleanup();
    }

#ifdef AUDIO_SYSTEM_ENABLED
    if (audioManager) {
        audioManager->cleanup();
    }
#endif

    LOGD("Renderer cleaned up");
}

bool Renderer::saveGameState(const std::string& slotName) {
    if (!saveManager) {
        LOGE("SaveManager not initialized");
        return false;
    }

    // Create current game state
    GameState state;
    state.saveName = slotName;
    state.saveTimestamp = std::time(nullptr);

    // Capture world state
    if (worldManager) {
        // BUG FIX: Use actual player position instead of hardcoded default
        state.playerPosition = worldManager->getPlayerPosition();

        // Capture NPC states
        NpcManager* npcMgr = worldManager->getNpcManager();
        if (npcMgr) {
            auto allNpcs = npcMgr->getAllNPCs();
            for (const auto& npc : allNpcs) {
                if (npc) {
                    state.npcStates[npc->npcId] =
                        std::make_pair(npc->position, npc->status);
                }
            }
        }
    }

    // Capture quest states
    if (questManager) {
        auto activeQuests = questManager->getActiveQuests();
        for (const auto& quest : activeQuests) {
            if (quest) {
                state.questStates[quest->questId] = static_cast<int>(quest->state);
            }
        }
    }

    // Save to file (legacy JSON format for UI compatibility)
    bool success = saveManager->saveGameLegacy(slotName, state);
    if (success) {
        LOGI("Game saved to slot: %s", slotName.c_str());
    } else {
        LOGE("Failed to save game to slot: %s", slotName.c_str());
    }
    return success;
}

void Renderer::toggleMap() {
    if (!mapUI) return;
    bool visible = !mapUI->isVisible();
    mapUI->setVisible(visible);
    if (visible) {
        mapUI->resetView();
        LOGI("World Map opened");
    } else {
        LOGI("World Map closed");
    }
}

void Renderer::toggleInventory() {
    if (!uiInventoryPanel) return;
    bool visible = !uiInventoryPanel->isVisible();
    uiInventoryPanel->setVisible(visible);
    if (visible) {
        LOGI("Inventory UI opened");
    } else {
        LOGI("Inventory UI closed");
    }
}

void Renderer::toggleGameConsole() {
    if (gameConsole) {
        gameConsole->toggle();
        LOGI("Game Console %s", gameConsole->isVisible() ? "opened" : "closed");
    }
}

bool Renderer::loadGameState(const std::string& slotName) {
    if (!saveManager) {
        LOGE("SaveManager not initialized");
        return false;
    }

    GameState state;
    bool success = saveManager->loadGameLegacy(slotName, state);

    if (!success) {
        LOGE("Failed to load game from slot: %s", slotName.c_str());
        return false;
    }

    // Restore NPC states
    if (worldManager && !state.npcStates.empty()) {
        NpcManager* npcMgr = worldManager->getNpcManager();
        if (npcMgr) {
            for (const auto& [npcId, positionStatus] : state.npcStates) {
                auto npc = npcMgr->getNPC(npcId);
                if (npc) {
                    npc->position = positionStatus.first;
                    npc->status = positionStatus.second;
                }
            }
        }
    }

    // BUG FIX: Restore player position from save state
    if (worldManager && playerController) {
        playerController->setPosition(state.playerPosition);
        LOGI("Player position restored to (%.1f, %.1f, %.1f)",
             state.playerPosition.x, state.playerPosition.y, state.playerPosition.z);
    }

    // Restore quest states
    if (questManager && !state.questStates.empty()) {
        for (const auto& [questId, questState] : state.questStates) {
            auto quest = questManager->getQuest(questId);
            if (quest) {
                quest->state = static_cast<QuestState>(questState);
            }
        }
    }

    LOGI("Game loaded from slot: %s", slotName.c_str());
    return true;
}

