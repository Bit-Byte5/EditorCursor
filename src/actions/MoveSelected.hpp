#pragma once

#include "EditorContext.hpp"

#include <Geode/Enums.hpp>

#include <optional>
#include <string>
#include <unordered_map>

namespace editorcursor {

inline std::optional<EditCommand> editCommandForDirection(
    std::string const& direction,
    std::string const& step
) {
    static std::unordered_map<std::string, EditCommand> const normal = {
        {"left", EditCommand::Left},
        {"right", EditCommand::Right},
        {"up", EditCommand::Up},
        {"down", EditCommand::Down},
    };
    static std::unordered_map<std::string, EditCommand> const smallStep = {
        {"left", EditCommand::SmallLeft},
        {"right", EditCommand::SmallRight},
        {"up", EditCommand::SmallUp},
        {"down", EditCommand::SmallDown},
    };
    static std::unordered_map<std::string, EditCommand> const bigStep = {
        {"left", EditCommand::BigLeft},
        {"right", EditCommand::BigRight},
        {"up", EditCommand::BigUp},
        {"down", EditCommand::BigDown},
    };
    static std::unordered_map<std::string, EditCommand> const tinyStep = {
        {"left", EditCommand::TinyLeft},
        {"right", EditCommand::TinyRight},
        {"up", EditCommand::TinyUp},
        {"down", EditCommand::TinyDown},
    };

    std::unordered_map<std::string, EditCommand> const* table = &normal;
    if (step == "small") table = &smallStep;
    else if (step == "big") table = &bigStep;
    else if (step == "tiny") table = &tinyStep;
    else if (step != "normal" && !step.empty()) return std::nullopt;

    auto it = table->find(direction);
    if (it == table->end()) return std::nullopt;
    return it->second;
}

inline matjson::Value runMoveSelected(
    LevelEditorLayer* editor,
    std::string const& nonce,
    char const* actionName,
    EditCommand command
) {
    auto access = openEditorAccess(editor, nonce, actionName);
    if (access.error) return *access.error;

    if (auto selectionError = requireSelection(access.ui, nonce, actionName)) {
        return *selectionError;
    }

    int selectedCount = countSelected(access.ui);
    auto offset = access.ui->moveForCommand(command);
    access.ui->moveObjectCall(command);

    return makeSuccess(
        nonce,
        "apply",
        actionName,
        matjson::makeObject({
            {"selectedCount", selectedCount},
            {"offset", matjson::makeObject({
                {"x", offset.x},
                {"y", offset.y},
            })},
            {"objects", collectSelectedPositions(access.ui)},
        })
    );
}

inline matjson::Value runMoveSelectedLeft(
    LevelEditorLayer* editor,
    std::string const& nonce,
    matjson::Value const&
) {
    return runMoveSelected(editor, nonce, "MOVE_SELECTED_LEFT", EditCommand::Left);
}

inline matjson::Value runMoveSelectedRight(
    LevelEditorLayer* editor,
    std::string const& nonce,
    matjson::Value const&
) {
    return runMoveSelected(editor, nonce, "MOVE_SELECTED_RIGHT", EditCommand::Right);
}

inline matjson::Value runMoveSelectedUp(
    LevelEditorLayer* editor,
    std::string const& nonce,
    matjson::Value const&
) {
    return runMoveSelected(editor, nonce, "MOVE_SELECTED_UP", EditCommand::Up);
}

inline matjson::Value runMoveSelectedDown(
    LevelEditorLayer* editor,
    std::string const& nonce,
    matjson::Value const&
) {
    return runMoveSelected(editor, nonce, "MOVE_SELECTED_DOWN", EditCommand::Down);
}

inline matjson::Value runNudgeSelected(
    LevelEditorLayer* editor,
    std::string const& nonce,
    matjson::Value const& request
) {
    if (!request.contains("direction") || !request["direction"].isString()) {
        return makeError(
            nonce,
            "parse",
            "MISSING_DIRECTION",
            "NUDGE_SELECTED requires a string direction field (left, right, up, down)",
            true,
            "NUDGE_SELECTED"
        );
    }

    auto direction = request["direction"].asString().unwrapOr("");
    auto step = request.contains("step") && request["step"].isString()
        ? request["step"].asString().unwrapOr("normal")
        : "normal";

    auto command = editCommandForDirection(direction, step);
    if (!command) {
        return makeError(
            nonce,
            "parse",
            "INVALID_DIRECTION",
            "direction must be left/right/up/down; step must be normal/small/big/tiny",
            true,
            "NUDGE_SELECTED"
        );
    }

    return runMoveSelected(editor, nonce, "NUDGE_SELECTED", *command);
}

} // namespace editorcursor
