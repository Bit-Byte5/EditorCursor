#pragma once

#include <Geode/Geode.hpp>
#include <matjson.hpp>
#include <functional>
#include <optional>
#include <string>

namespace editorcursor {

inline matjson::Value makeSuccess(
    std::string const& nonce,
    std::string const& stage,
    std::string const& action,
    matjson::Value response = matjson::Value()
) {
    return matjson::makeObject({
        {"status", "successful"},
        {"nonce", nonce},
        {"stage", stage},
        {"action", action},
        {"response", std::move(response)},
    });
}

inline matjson::Value makeError(
    std::string const& nonce,
    std::string const& stage,
    std::string const& errorCode,
    std::string const& error,
    bool retryable,
    std::optional<std::string> action = std::nullopt
) {
    matjson::Value obj = matjson::makeObject({
        {"status", "error"},
        {"stage", stage},
        {"errorCode", errorCode},
        {"error", error},
        {"retryable", retryable},
    });

    if (!nonce.empty()) {
        obj["nonce"] = nonce;
    } else {
        obj["nonce"] = nullptr;
    }

    if (action) {
        obj["action"] = *action;
    }

    return obj;
}

inline std::string serialize(matjson::Value const& value) {
    try {
        return value.dump(matjson::NO_INDENTATION);
    } catch (...) {
        return R"({"status":"error","stage":"queue","errorCode":"SERIALIZE_FAILED","error":"Failed to serialize response","retryable":false})";
    }
}

inline void safeSend(
    std::function<void(std::string const&)> const& send,
    matjson::Value const& response
) {
    try {
        send(serialize(response));
    } catch (std::exception const& e) {
        geode::log::error("[EditorCursor] safeSend exception: {}", e.what());
    } catch (...) {
        geode::log::error("[EditorCursor] safeSend unknown exception");
    }
}

} // namespace editorcursor
