#pragma once

#include "../Response.hpp"

#include <Geode/Geode.hpp>

#include <functional>

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

inline matjson::Value objectSnapshot(GameObject* obj) {
    if (!obj) return matjson::Value();

    auto pos = obj->getPosition();
    return matjson::makeObject({
        {"uniqueId", obj->m_uniqueID},
        {"objectId", obj->m_objectID},
        {"x", pos.x},
        {"y", pos.y},
    });
}

inline matjson::Value collectObjectGroups(GameObject* obj) {
    matjson::Value groups = matjson::Value::array();
    if (!obj) return groups;

    int const count = obj->m_groupCount > 0 ? obj->m_groupCount : 10;
    for (int i = 0; i < count && i < 10; i++) {
        int const groupId = obj->getGroupID(i);
        if (groupId > 0) {
            groups.push(groupId);
        }
    }

    return groups;
}

inline bool objectHasGroup(GameObject* obj, int groupId) {
    if (!obj || groupId <= 0) return false;

    int const count = obj->m_groupCount > 0 ? obj->m_groupCount : 10;
    for (int i = 0; i < count && i < 10; i++) {
        if (obj->getGroupID(i) == groupId) {
            return true;
        }
    }

    return false;
}

inline matjson::Value objectQuerySnapshot(GameObject* obj) {
    auto snapshot = objectSnapshot(obj);

    auto groups = collectObjectGroups(obj);
    if (groups.size() > 0) {
        snapshot["groups"] = std::move(groups);
    }

    if (obj->m_isTrigger) {
        snapshot["isTrigger"] = true;
    }

    if (auto* effect = typeinfo_cast<EffectGameObject*>(obj)) {
        if (effect->m_targetGroupID > 0) {
            snapshot["targetGroupId"] = effect->m_targetGroupID;
        }
    }

    return snapshot;
}

inline void forEachLevelObject(
    LevelEditorLayer* editor,
    std::function<void(GameObject*)> const& visit
) {
    if (!editor) return;

    auto* objects = editor->getAllObjects();
    if (!objects) return;

    for (auto* obj : CCArrayExt<GameObject>(objects)) {
        visit(obj);
    }
}

inline matjson::Value collectSelectedPositions(EditorUI* ui) {
    matjson::Value positions = matjson::Value::array();
    if (!ui) return positions;

    auto appendPosition = [&](GameObject* obj) {
        if (!obj) return;
        positions.push(objectSnapshot(obj));
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
