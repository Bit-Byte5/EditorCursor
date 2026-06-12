#pragma once

#include <Geode/Geode.hpp>

#include <optional>
#include <vector>

namespace editorcursor {

inline std::optional<int> readIntField(
    matjson::Value const& request,
    char const* field
) {
    if (!request.contains(field)) return std::nullopt;

    auto const& value = request[field];
    if (!value.isNumber()) return std::nullopt;
    return static_cast<int>(value.asDouble().unwrapOr(0.0));
}

inline std::optional<double> readDoubleField(
    matjson::Value const& request,
    char const* field
) {
    if (!request.contains(field)) return std::nullopt;

    auto const& value = request[field];
    if (!value.isNumber()) return std::nullopt;
    return value.asDouble().unwrapOr(0.0);
}

inline bool readBoolField(
    matjson::Value const& request,
    char const* field,
    bool defaultValue
) {
    if (!request.contains(field) || !request[field].isBool()) {
        return defaultValue;
    }
    return request[field].asBool().unwrapOr(defaultValue);
}

inline std::vector<int> readUniqueIdList(matjson::Value const& request) {
    std::vector<int> ids;

    if (auto single = readIntField(request, "uniqueId")) {
        ids.push_back(*single);
        return ids;
    }

    if (request.contains("uniqueIds") && request["uniqueIds"].isArray()) {
        for (auto const& item : request["uniqueIds"]) {
            if (!item.isNumber()) continue;
            ids.push_back(static_cast<int>(item.asDouble().unwrapOr(0.0)));
        }
    }

    return ids;
}

} // namespace editorcursor
