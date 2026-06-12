#pragma once

#include "Response.hpp"
#include "actions/CreateObject.hpp"
#include "actions/MoveSelected.hpp"
#include "actions/QueryLevel.hpp"
#include "actions/SelectObjects.hpp"

#include <Geode/Geode.hpp>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <websocketpp/common/connection_hdl.hpp>

using websocketpp::connection_hdl;

namespace editorcursor {

using ActionHandler = std::function<matjson::Value(
    LevelEditorLayer*,
    std::string const&,
    matjson::Value const&
)>;

struct ActionDefinition {
    char const* name;
    bool requiresEditor;
    ActionHandler run;
};

struct QueuedAction {
    connection_hdl hdl;
    std::string nonce;
    std::string action;
    matjson::Value request;
};

inline std::atomic<bool> g_inEditor{false};
inline std::vector<QueuedAction> g_actions;
inline std::mutex g_actionsMutex;

inline std::unordered_map<std::string, ActionDefinition> buildActionRegistry() {
    return {
        {"MOVE_SELECTED_LEFT", {"MOVE_SELECTED_LEFT", true, runMoveSelectedLeft}},
        {"MOVE_SELECTED_RIGHT", {"MOVE_SELECTED_RIGHT", true, runMoveSelectedRight}},
        {"MOVE_SELECTED_UP", {"MOVE_SELECTED_UP", true, runMoveSelectedUp}},
        {"MOVE_SELECTED_DOWN", {"MOVE_SELECTED_DOWN", true, runMoveSelectedDown}},
        {"NUDGE_SELECTED", {"NUDGE_SELECTED", true, runNudgeSelected}},
        {"CREATE_OBJECT", {"CREATE_OBJECT", true, runCreateObject}},
        {"GET_SELECTION", {"GET_SELECTION", true, runGetSelection}},
        {"GET_LEVEL_SUMMARY", {"GET_LEVEL_SUMMARY", true, runGetLevelSummary}},
        {"QUERY_OBJECTS", {"QUERY_OBJECTS", true, runQueryObjects}},
        {"SELECT_OBJECTS", {"SELECT_OBJECTS", true, runSelectObjects}},
        {"DESELECT_ALL", {"DESELECT_ALL", true, runDeselectAll}},
    };
}

inline ActionDefinition const* findActionDefinition(std::string const& action) {
    static auto const registry = buildActionRegistry();
    auto it = registry.find(action);
    if (it == registry.end()) return nullptr;
    return &it->second;
}

inline void queueAction(
    connection_hdl hdl,
    std::string nonce,
    std::string action,
    matjson::Value request = matjson::Value()
) {
    std::lock_guard lock(g_actionsMutex);
    g_actions.push_back(QueuedAction{
        .hdl = hdl,
        .nonce = std::move(nonce),
        .action = std::move(action),
        .request = std::move(request),
    });
}

inline matjson::Value makeNotInEditorError(
    std::string const& nonce,
    char const* action
) {
    return makeError(
        nonce,
        "editor_gate",
        "NOT_IN_EDITOR",
        "Enter the level editor to run this action",
        true,
        action
    );
}

inline void drainActionQueue(
    LevelEditorLayer* editor,
    std::function<void(connection_hdl const&, std::string const&)> const& send
) {
    std::vector<QueuedAction> batch;

    {
        std::lock_guard lock(g_actionsMutex);
        if (g_actions.empty()) return;
        batch.swap(g_actions);
    }

    for (auto& item : batch) {
        try {
            auto const* def = findActionDefinition(item.action);
            if (!def) {
                safeSend(
                    [&](std::string const& json) { send(item.hdl, json); },
                    makeError(
                        item.nonce,
                        "execute",
                        "UNKNOWN_ACTION",
                        "Queued action is no longer supported",
                        false,
                        item.action
                    )
                );
                continue;
            }

            auto result = def->run(editor, item.nonce, item.request);
            safeSend([&](std::string const& json) { send(item.hdl, json); }, std::move(result));
        } catch (std::exception const& e) {
            geode::log::error("[EditorCursor] drainActionQueue exception: {}", e.what());
            safeSend(
                [&](std::string const& json) { send(item.hdl, json); },
                makeError(
                    item.nonce,
                    "execute",
                    "INTERNAL_ERROR",
                    "An unexpected error occurred while executing the action",
                    true,
                    item.action
                )
            );
        } catch (...) {
            geode::log::error("[EditorCursor] drainActionQueue unknown exception");
            safeSend(
                [&](std::string const& json) { send(item.hdl, json); },
                makeError(
                    item.nonce,
                    "execute",
                    "INTERNAL_ERROR",
                    "An unexpected error occurred while executing the action",
                    true,
                    item.action
                )
            );
        }
    }
}

} // namespace editorcursor
