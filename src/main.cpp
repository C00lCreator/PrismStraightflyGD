#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <cmath>
#include <deque>

using namespace geode::prelude;

namespace sfh {
    struct State {
        bool syntheticInput = false;
        bool physicalHeld = false;
        bool assistHeld = false;
        bool active = false;

        float targetY = 0.f;
        float activeSeconds = 0.f;
        float sinceLastPhysicalChange = 999.f;
        float previousY = 0.f;
        float estimatedVelocityY = 0.f;

        std::deque<float> inputChangeAges;
        std::deque<float> recentY;
        CCLabelBMFont* statusLabel = nullptr;

        void resetDetection() {
            inputChangeAges.clear();
            recentY.clear();
            sinceLastPhysicalChange = 999.f;
        }

        void stopAssist(GJBaseGameLayer* layer) {
            if (assistHeld && layer) {
                syntheticInput = true;
                layer->handleButton(false, 1, true);
                syntheticInput = false;
            }

            assistHeld = false;
            active = false;
            activeSeconds = 0.f;

            if (statusLabel) {
                statusLabel->setVisible(false);
            }
        }
    };

    State g_state;

    bool settingBool(char const* key, bool fallback) {
        auto mod = Mod::get();
        if (!mod) return fallback;
        try { return mod->getSettingValue<bool>(key); }
        catch (...) { return fallback; }
    }

    float settingFloat(char const* key, float fallback) {
        auto mod = Mod::get();
        if (!mod) return fallback;
        try { return static_cast<float>(mod->getSettingValue<double>(key)); }
        catch (...) {
            try { return mod->getSettingValue<float>(key); }
            catch (...) { return fallback; }
        }
    }

    int settingInt(char const* key, int fallback) {
        auto mod = Mod::get();
        if (!mod) return fallback;
        try { return static_cast<int>(mod->getSettingValue<int64_t>(key)); }
        catch (...) {
            try { return mod->getSettingValue<int>(key); }
            catch (...) { return fallback; }
        }
    }

    bool isPracticeShip(PlayLayer* layer) {
        return layer &&
               layer->m_isPracticeMode &&
               layer->m_player1 &&
               layer->m_player1->m_isShip;
    }

    void ensureStatusLabel(PlayLayer* layer) {
        if (!layer || g_state.statusLabel) return;

        auto label = CCLabelBMFont::create(
            "PRACTICE STRAIGHT FLY ASSIST",
            "bigFont.fnt"
        );
        if (!label) return;

        label->setScale(0.36f);
        label->setOpacity(185);
        label->setColor({255, 210, 70});

        auto size = CCDirector::sharedDirector()->getWinSize();
        label->setPosition({size.width / 2.f, size.height - 18.f});
        label->setZOrder(1000);
        label->setVisible(false);

        layer->addChild(label);
        g_state.statusLabel = label;
    }

    bool looksLikeStraightFlyAttempt() {
        int requiredChanges = settingInt("detection-taps", 5);
        float window = settingFloat("detection-window", 1.25f);

        int recentChanges = 0;
        for (float age : g_state.inputChangeAges) {
            if (age <= window) recentChanges++;
        }

        if (recentChanges < requiredChanges || g_state.recentY.size() < 4) {
            return false;
        }

        auto [minY, maxY] = std::minmax_element(
            g_state.recentY.begin(),
            g_state.recentY.end()
        );

        // A straight-fly attempt usually has rapid alternating input and a
        // comparatively small vertical range.
        return (*maxY - *minY) <= 28.f;
    }

    void setAssistInput(GJBaseGameLayer* layer, bool held) {
        if (!layer || held == g_state.assistHeld) return;

        g_state.syntheticInput = true;
        layer->handleButton(held, 1, true);
        g_state.syntheticInput = false;
        g_state.assistHeld = held;
    }
}

class $modify(SFHInputLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool player1) {
        // Record only real player-1 jump input. Synthetic correction input is
        // ignored so it cannot retrigger detection.
        if (!sfh::g_state.syntheticInput && player1 && button == 1) {
            if (down != sfh::g_state.physicalHeld) {
                sfh::g_state.physicalHeld = down;
                sfh::g_state.sinceLastPhysicalChange = 0.f;
                sfh::g_state.inputChangeAges.push_back(0.f);

                while (sfh::g_state.inputChangeAges.size() > 24) {
                    sfh::g_state.inputChangeAges.pop_front();
                }
            }

            // Physical input always takes priority immediately.
            if (sfh::g_state.active) {
                sfh::g_state.assistHeld = down;
            }
        }

        GJBaseGameLayer::handleButton(down, button, player1);
    }
};

class $modify(SFHPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        sfh::g_state = {};
        sfh::ensureStatusLabel(this);
        return true;
    }

    void resetLevel() {
        sfh::g_state.stopAssist(this);
        sfh::g_state.resetDetection();
        sfh::g_state.statusLabel = nullptr;

        PlayLayer::resetLevel();

        sfh::ensureStatusLabel(this);
    }

    void onQuit() {
        sfh::g_state.stopAssist(this);
        sfh::g_state.statusLabel = nullptr;
        PlayLayer::onQuit();
    }

    void update(float dt) {
        PlayLayer::update(dt);

        auto& state = sfh::g_state;
        state.sinceLastPhysicalChange += dt;

        for (float& age : state.inputChangeAges) {
            age += dt;
        }

        float detectionWindow = sfh::settingFloat("detection-window", 1.25f);
        while (!state.inputChangeAges.empty() &&
               state.inputChangeAges.front() > detectionWindow * 1.8f) {
            state.inputChangeAges.pop_front();
        }

        if (!sfh::settingBool("enabled", true) || !sfh::isPracticeShip(this)) {
            state.stopAssist(this);
            state.resetDetection();
            return;
        }

        auto player = this->m_player1;
        float currentY = player->getPositionY();

        if (state.previousY == 0.f) {
            state.previousY = currentY;
        }

        if (dt > 0.0001f) {
            float rawVelocity = (currentY - state.previousY) / dt;
            state.estimatedVelocityY =
                state.estimatedVelocityY * 0.72f + rawVelocity * 0.28f;
        }
        state.previousY = currentY;

        state.recentY.push_back(currentY);
        while (state.recentY.size() > 45) {
            state.recentY.pop_front();
        }

        if (!state.active &&
            sfh::settingBool("auto-detect", true) &&
            sfh::looksLikeStraightFlyAttempt()) {

            float sum = 0.f;
            for (float y : state.recentY) sum += y;
            state.targetY = sum / static_cast<float>(state.recentY.size());

            state.active = true;
            state.activeSeconds = 0.f;
            state.assistHeld = state.physicalHeld;

            if (state.statusLabel &&
                sfh::settingBool("show-status", true)) {
                state.statusLabel->setVisible(true);
            }

            Notification::create(
                "Straight-fly assist detected (Practice Mode)",
                NotificationIcon::Info,
                1.4f
            )->show();
        }

        if (!state.active) return;

        state.activeSeconds += dt;

        float maxSeconds = sfh::settingFloat("max-assist-seconds", 8.f);
        if (state.activeSeconds >= maxSeconds ||
            state.sinceLastPhysicalChange > 1.15f) {
            state.stopAssist(this);
            state.resetDetection();
            return;
        }

        float deadzone = sfh::settingFloat("deadzone", 2.8f);
        float velocityStrength =
            sfh::settingFloat("velocity-strength", 0.18f);

        float positionError = state.targetY - currentY;
        float correction =
            positionError - state.estimatedVelocityY * velocityStrength;

        bool desiredHeld = state.assistHeld;

        if (correction > deadzone) {
            desiredHeld = true;
        }
        else if (correction < -deadzone) {
            desiredHeld = false;
        }
        else {
            // Inside the deadzone, follow real input. This keeps the feature
            // assistive rather than fully autonomous.
            desiredHeld = state.physicalHeld;
        }

        sfh::setAssistInput(this, desiredHeld);

        if (state.statusLabel) {
            state.statusLabel->setVisible(
                sfh::settingBool("show-status", true)
            );
        }
    }
};