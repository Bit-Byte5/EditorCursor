#pragma once

#include "EditorContext.hpp"
#include "RequestFields.hpp"

namespace editorcursor {

inline matjson::Value runCreateObject(
    LevelEditorLayer* editor,
    std::string const& nonce,
    matjson::Value const& request
) {
    auto access = openEditorAccess(editor, nonce, "CREATE_OBJECT");
    if (access.error) return *access.error;

    auto objectId = readIntField(request, "objectId");
    if (!objectId) {
        objectId = readIntField(request, "objectID");
    }
    if (!objectId) {
        return makeError(
            nonce,
            "parse",
            "MISSING_OBJECT_ID",
            "CREATE_OBJECT requires an integer objectId field",
            true,
            "CREATE_OBJECT"
        );
    }
    if (*objectId <= 0) {
        return makeError(
            nonce,
            "parse",
            "INVALID_OBJECT_ID",
            "objectId must be a positive integer",
            true,
            "CREATE_OBJECT"
        );
    }

    auto x = readDoubleField(request, "x");
    auto y = readDoubleField(request, "y");
    if (!x || !y) {
        return makeError(
            nonce,
            "parse",
            "MISSING_POSITION",
            "CREATE_OBJECT requires numeric x and y fields",
            true,
            "CREATE_OBJECT"
        );
    }

    auto position = cocos2d::CCPoint{
        static_cast<float>(*x),
        static_cast<float>(*y),
    };

    if (readBoolField(request, "snapToGrid", true)) {
        position = access.ui->getGridSnappedPos(position);
    }

    auto* created = access.ui->createObject(*objectId, position);
    if (!created) {
        return makeError(
            nonce,
            "execute",
            "CREATE_FAILED",
            "The editor could not create an object with the given objectId",
            true,
            "CREATE_OBJECT"
        );
    }

    if (readBoolField(request, "select", true)) {
        access.ui->selectObject(created, false);
    }

    return makeSuccess(
        nonce,
        "apply",
        "CREATE_OBJECT",
        matjson::makeObject({
            {"object", objectSnapshot(created)},
        })
    );
}

} // namespace editorcursor
