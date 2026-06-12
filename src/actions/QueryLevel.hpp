#pragma once

#include "EditorContext.hpp"
#include "RequestFields.hpp"

#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace editorcursor {

struct QueryRect {
    float minX;
    float minY;
    float maxX;
    float maxY;
};

inline std::optional<QueryRect> readQueryRect(matjson::Value const& request) {
    if (!request.contains("rect") || !request["rect"].isObject()) {
        return std::nullopt;
    }

    auto const& rect = request["rect"];
    auto minX = readDoubleField(rect, "minX");
    auto minY = readDoubleField(rect, "minY");
    auto maxX = readDoubleField(rect, "maxX");
    auto maxY = readDoubleField(rect, "maxY");

    if (!minX || !minY || !maxX || !maxY) {
        return std::nullopt;
    }

    if (*minX > *maxX || *minY > *maxY) {
        return std::nullopt;
    }

    return QueryRect{
        static_cast<float>(*minX),
        static_cast<float>(*minY),
        static_cast<float>(*maxX),
        static_cast<float>(*maxY),
    };
}

inline bool objectInQueryRect(GameObject* obj, QueryRect const& rect) {
    if (!obj) return false;

    auto pos = obj->getPosition();
    return pos.x >= rect.minX && pos.x <= rect.maxX
        && pos.y >= rect.minY && pos.y <= rect.maxY;
}

inline matjson::Value runGetLevelSummary(
    LevelEditorLayer* editor,
    std::string const& nonce,
    matjson::Value const&
) {
    auto access = openEditorAccess(editor, nonce, "GET_LEVEL_SUMMARY");
    if (access.error) return *access.error;

    int totalObjectCount = 0;
    int triggerCount = 0;
    std::unordered_map<int, int> objectIdCounts;
    std::unordered_set<int> groupIds;

    bool hasBounds = false;
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    forEachLevelObject(access.editor, [&](GameObject* obj) {
        if (!obj) return;

        totalObjectCount++;
        objectIdCounts[obj->m_objectID]++;

        if (obj->m_isTrigger) {
            triggerCount++;
        }

        int const groupCount = obj->m_groupCount > 0 ? obj->m_groupCount : 10;
        for (int i = 0; i < groupCount && i < 10; i++) {
            int const groupId = obj->getGroupID(i);
            if (groupId > 0) {
                groupIds.insert(groupId);
            }
        }

        auto pos = obj->getPosition();
        minX = std::min(minX, pos.x);
        minY = std::min(minY, pos.y);
        maxX = std::max(maxX, pos.x);
        maxY = std::max(maxY, pos.y);
        hasBounds = true;
    });

    matjson::Value objectIdCountsJson = matjson::Value::object();
    for (auto const& [objectId, count] : objectIdCounts) {
        objectIdCountsJson[std::to_string(objectId)] = count;
    }

    matjson::Value groupIdsJson = matjson::Value::array();
    for (int groupId : groupIds) {
        groupIdsJson.push(groupId);
    }

    matjson::Value response = matjson::makeObject({
        {"totalObjectCount", totalObjectCount},
        {"selectedCount", countSelected(access.ui)},
        {"triggerCount", triggerCount},
        {"uniqueObjectIdTypes", static_cast<int>(objectIdCounts.size())},
        {"uniqueGroupCount", static_cast<int>(groupIds.size())},
        {"objectIdCounts", std::move(objectIdCountsJson)},
        {"groupIds", std::move(groupIdsJson)},
    });

    if (hasBounds) {
        response["bounds"] = matjson::makeObject({
            {"minX", minX},
            {"minY", minY},
            {"maxX", maxX},
            {"maxY", maxY},
        });
    }

    return makeSuccess(
        nonce,
        "read",
        "GET_LEVEL_SUMMARY",
        std::move(response)
    );
}

inline matjson::Value runQueryObjects(
    LevelEditorLayer* editor,
    std::string const& nonce,
    matjson::Value const& request
) {
    auto access = openEditorAccess(editor, nonce, "QUERY_OBJECTS");
    if (access.error) return *access.error;

    auto objectIdFilter = readIntField(request, "objectId");
    auto groupIdFilter = readIntField(request, "groupId");
    auto targetGroupIdFilter = readIntField(request, "targetGroupId");
    auto rectFilter = readQueryRect(request);

    if (request.contains("rect") && !rectFilter) {
        return makeError(
            nonce,
            "parse",
            "INVALID_RECT",
            "rect must be an object with numeric minX, minY, maxX, maxY where min <= max",
            true,
            "QUERY_OBJECTS"
        );
    }

    bool const hasFilter = objectIdFilter || groupIdFilter || targetGroupIdFilter || rectFilter;
    if (!hasFilter) {
        return makeError(
            nonce,
            "parse",
            "MISSING_FILTER",
            "QUERY_OBJECTS requires at least one filter: objectId, groupId, targetGroupId, or rect",
            true,
            "QUERY_OBJECTS"
        );
    }

    int limit = readIntField(request, "limit").value_or(50);
    if (limit <= 0) {
        return makeError(
            nonce,
            "parse",
            "INVALID_LIMIT",
            "limit must be a positive integer",
            true,
            "QUERY_OBJECTS"
        );
    }
    limit = std::min(limit, 200);

    matjson::Value objects = matjson::Value::array();
    int matchCount = 0;
    bool truncated = false;

    forEachLevelObject(access.editor, [&](GameObject* obj) {
        if (!obj) return;

        if (objectIdFilter && obj->m_objectID != *objectIdFilter) {
            return;
        }

        if (groupIdFilter && !objectHasGroup(obj, *groupIdFilter)) {
            return;
        }

        if (targetGroupIdFilter) {
            auto* effect = typeinfo_cast<EffectGameObject*>(obj);
            if (!effect || effect->m_targetGroupID != *targetGroupIdFilter) {
                return;
            }
        }

        if (rectFilter && !objectInQueryRect(obj, *rectFilter)) {
            return;
        }

        matchCount++;
        if (static_cast<int>(objects.size()) < limit) {
            objects.push(objectQuerySnapshot(obj));
        } else {
            truncated = true;
        }
    });

    return makeSuccess(
        nonce,
        "read",
        "QUERY_OBJECTS",
        matjson::makeObject({
            {"matchCount", matchCount},
            {"returnedCount", static_cast<int>(objects.size())},
            {"truncated", truncated || matchCount > limit},
            {"limit", limit},
            {"objects", std::move(objects)},
        })
    );
}

} // namespace editorcursor
