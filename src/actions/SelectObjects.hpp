#pragma once

#include "EditorContext.hpp"
#include "RequestFields.hpp"

#include <unordered_set>

using namespace geode::prelude;

namespace editorcursor {

inline matjson::Value runGetSelection(
    LevelEditorLayer* editor,
    std::string const& nonce,
    matjson::Value const&
) {
    auto access = openEditorAccess(editor, nonce, "GET_SELECTION");
    if (access.error) return *access.error;

    return makeSuccess(
        nonce,
        "read",
        "GET_SELECTION",
        matjson::makeObject({
            {"selectedCount", countSelected(access.ui)},
            {"objects", collectSelectedPositions(access.ui)},
        })
    );
}

inline matjson::Value runDeselectAll(
    LevelEditorLayer* editor,
    std::string const& nonce,
    matjson::Value const&
) {
    auto access = openEditorAccess(editor, nonce, "DESELECT_ALL");
    if (access.error) return *access.error;

    access.ui->deselectAll();

    return makeSuccess(
        nonce,
        "apply",
        "DESELECT_ALL",
        matjson::makeObject({
            {"selectedCount", 0},
        })
    );
}

inline matjson::Value runSelectObjects(
    LevelEditorLayer* editor,
    std::string const& nonce,
    matjson::Value const& request
) {
    auto access = openEditorAccess(editor, nonce, "SELECT_OBJECTS");
    if (access.error) return *access.error;

    auto uniqueIds = readUniqueIdList(request);
    if (uniqueIds.empty()) {
        return makeError(
            nonce,
            "parse",
            "MISSING_UNIQUE_IDS",
            "SELECT_OBJECTS requires uniqueId or a non-empty uniqueIds array",
            true,
            "SELECT_OBJECTS"
        );
    }

    std::vector<GameObject*> found;
    matjson::Value missing = matjson::Value::array();

    for (int uniqueId : uniqueIds) {
        auto* obj = access.editor->findGameObject(uniqueId);
        if (obj) {
            found.push_back(obj);
        } else {
            missing.push(uniqueId);
        }
    }

    if (found.empty()) {
        return makeError(
            nonce,
            "execute",
            "OBJECT_NOT_FOUND",
            "None of the requested uniqueIds exist in the level",
            true,
            "SELECT_OBJECTS"
        );
    }

    bool const additive = readBoolField(request, "additive", false);
    bool const ignoreFilter = readBoolField(request, "ignoreFilter", false);

    if (!additive) {
        access.ui->deselectAll();
    }

    if (additive) {
        auto* arr = CCArray::create();
        std::unordered_set<int> selectedIds;

        auto appendUnique = [&](GameObject* obj) {
            if (!obj) return;
            if (!selectedIds.insert(obj->m_uniqueID).second) return;
            arr->addObject(obj);
        };

        if (access.ui->m_selectedObject) {
            appendUnique(access.ui->m_selectedObject);
        }
        if (access.ui->m_selectedObjects) {
            for (auto* obj : CCArrayExt<GameObject>(access.ui->m_selectedObjects)) {
                appendUnique(obj);
            }
        }
        for (auto* obj : found) {
            appendUnique(obj);
        }

        access.ui->selectObjects(arr, ignoreFilter);
    } else if (found.size() == 1) {
        access.ui->selectObject(found.front(), ignoreFilter);
    } else {
        auto* arr = CCArray::create();
        for (auto* obj : found) {
            arr->addObject(obj);
        }
        access.ui->selectObjects(arr, ignoreFilter);
    }

    matjson::Value response = matjson::makeObject({
        {"selectedCount", countSelected(access.ui)},
        {"objects", collectSelectedPositions(access.ui)},
    });

    if (missing.size() > 0) {
        response["missingUniqueIds"] = std::move(missing);
    }

    return makeSuccess(
        nonce,
        "apply",
        "SELECT_OBJECTS",
        std::move(response)
    );
}

} // namespace editorcursor
