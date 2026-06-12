#include "ActionRegistry.hpp"
#include "WsServer.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>

#include <cstdlib>

using namespace geode::prelude;
using namespace editorcursor;

$on_mod(Loaded) {
    auto* mod = Mod::get();
    if (!mod) return;

    int port = mod->getSettingValue<int>("ws-port");
    if (port < 1024 || port > 65535) {
        port = 1314;
    }

    startWsServer(port);
    std::atexit(+[]() { stopWsServer(); });
}

class $modify(EditorCursorHooks, LevelEditorLayer) {
    struct Fields {
        ~Fields() {
            g_inEditor.store(false);
        }
    };

    void drainQueuedActions() {
        try {
            drainActionQueue(this, +[](websocketpp::connection_hdl const& hdl, std::string const& payload) {
                sendToClient(hdl, payload);
            });
        } catch (std::exception const& e) {
            log::error("[EditorCursor] drainQueuedActions exception: {}", e.what());
        } catch (...) {
            log::error("[EditorCursor] drainQueuedActions unknown exception");
        }
    }

    bool init(GJGameLevel* level, bool p1) {
        if (!LevelEditorLayer::init(level, p1)) {
            return false;
        }

        g_inEditor.store(true);
        return true;
    }

    void postUpdate(float dt) {
        LevelEditorLayer::postUpdate(dt);
        drainQueuedActions();
    }
};
