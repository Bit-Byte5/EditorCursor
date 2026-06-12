#pragma once

#include "../Response.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace editorcursor {

struct EditorAccess {
    LevelEditorLayer* editor = nullptr;
    EditorUI* ui = nullptr;
    std::optional<matjson::Value> error;
};

inline int countSelected(EditorUI* ui) {
    if (!ui) return 0;

    int count = 0;
    if (ui->m_selectedObject) count++;
    if (ui->m_selectedObjects) {
        count += static_cast<int>(ui->m_selectedObjects->count());
    }
    return count;
}

inline matjson::Value collectSelectedPositions(EditorUI* ui) {
    matjson::Value positions = matjson::Value::array();
    if (!ui) return positions;

    auto appendPosition = [&](GameObject* obj) {
        if (!obj) return;
        auto pos = obj->getPosition();
        positions.push(matjson::makeObject({
            {"uniqueId", obj->m_uniqueID},
            {"objectId", obj->m_objectID},
            {"x", pos.x},
            {"y", pos.y},
        }));
    };

    if (ui->m_selectedObject) {
        appendPosition(ui->m_selectedObject);
    }
    if (ui->m_selectedObjects) {
        for (auto* obj : CCArrayExt<GameObject>(ui->m_selectedObjects)) {
            appendPosition(obj);
        }
    }

    return positions;
}

inline EditorAccess openEditorAccess(
    LevelEditorLayer* editor,
    std::string const& nonce,
    char const* action
) {
    EditorAccess access;

    if (!editor) {
        access.error = makeError(
            nonce,
            "execute",
            "EDITOR_LAYER_LOST",
            "Level editor layer is not available",
            true,
            action
        );
        return access;
    }

    access.editor = editor;
    access.ui = editor->m_editorUI;
    if (!access.ui) {
        access.error = makeError(
            nonce,
            "execute",
            "EDITOR_UI_MISSING",
            "Editor UI is not available",
            true,
            action
        );
    }

    return access;
}

inline std::optional<matjson::Value> requireSelection(
    EditorUI* ui,
    std::string const& nonce,
    char const* action
) {
    if (countSelected(ui) > 0) {
        return std::nullopt;
    }

    return makeError(
        nonce,
        "execute",
        "NO_SELECTION",
        "No object selected in the editor",
        true,
        action
    );
}

} // namespace editorcursor
