# EditorCursor — Agent Handoff

This document captures everything a new agent needs to continue work on **EditorCursor** without re-discovering architecture, gotchas, and domain knowledge from scratch.

## What this project is

**EditorCursor** is a Geode mod (`paxcirlot.editorcursor`) for Geometry Dash **2.2081** that exposes a **localhost-only WebSocket API** so external tools (primarily Cursor IDE) can read and mutate the **level editor** programmatically.

- **Repo:** `/Users/paxcirlot/EditorCursor`
- **Mod ID:** `paxcirlot.editorcursor`
- **Default WS URL:** `ws://127.0.0.1:1314`
- **Developer machine:** Mac, Steam GD, Geode 5.7.1
- **Geode SDK path (when not in shell env):** `/Users/Shared/Geode/sdk`

## Architecture

```
External client (Cursor, wscat, script)
        │  JSON request
        ▼
WsServer.cpp (background thread)
  - parse JSON, validate nonce + action
  - PING → immediate reply
  - everything else → queueAction()
        │
        ▼
ActionRegistry.hpp (g_actions queue + mutex)
        │
        ▼
main.cpp → LevelEditorLayer::postUpdate (main/game thread, every frame)
  - drainActionQueue()
  - calls action handler → editor APIs
  - sendToClient() with JSON response
```

### Threading rules (critical)

- **Never** call GD editor APIs from the WebSocket thread.
- All editor mutations and reads go through the queue, drained on the **main thread** via `postUpdate`.
- **Do not use `schedule_selector` on `$modify` classes** — it did not fire; queue drain was broken until we hooked `postUpdate`.

### Response envelope

Success:

```json
{
  "status": "successful",
  "nonce": "...",
  "stage": "ping|read|apply|...",
  "action": "ACTION_NAME",
  "response": { }
}
```

Error:

```json
{
  "status": "error",
  "nonce": "...",
  "stage": "...",
  "errorCode": "NOT_IN_EDITOR",
  "error": "human message",
  "retryable": true,
  "action": "OPTIONAL"
}
```

Every request **must** include a non-empty string `"nonce"`.

## Build and install

```bash
cd /Users/paxcirlot/EditorCursor
GEODE_SDK=/Users/Shared/Geode/sdk geode build
```

- Builds `paxcirlot.editorcursor.geode` and **auto-installs** into the GD Geode mods folder.
- **Restart GD** (or reload mods) after every build — the running process does not hot-reload the dylib.
- Mod icon: `logo.png` in project root (256×256 recommended). Packaged automatically into `.geode`.
- If icon does not appear: restart GD; logo loads from `geode/unzipped/paxcirlot.editorcursor/logo.png`.

### Geode 5.7.1 notes already applied

- Settings: `mod->getSettingValue<int>("ws-port")` (not old `int64_t` + `unwrapOr`)
- No `$on_mod(Unloaded)` — cleanup via `std::atexit(stopWsServer)`
- `$on_mod(Loaded)` starts WebSocket server

## Source layout

| Path | Role |
|------|------|
| `src/main.cpp` | Mod load, `LevelEditorLayer` hook, `postUpdate` queue drain |
| `src/WsServer.cpp` | WebSocket server, message routing, `sendToClient` |
| `src/ActionRegistry.hpp` | Action registry, queue, drain — **register new actions here** |
| `src/Response.hpp` | `makeSuccess`, `makeError`, `safeSend`, `serialize` |
| `src/actions/EditorContext.hpp` | `openEditorAccess`, `objectSnapshot`, selection helpers |
| `src/actions/RequestFields.hpp` | Shared JSON field parsers (`readIntField`, etc.) |
| `src/actions/MoveSelected.hpp` | Move / nudge selected objects |
| `src/actions/CreateObject.hpp` | `CREATE_OBJECT` |
| `src/actions/SelectObjects.hpp` | `GET_SELECTION`, `SELECT_OBJECTS`, `DESELECT_ALL` |
| `src/actions/QueryLevel.hpp` | `GET_LEVEL_SUMMARY`, `QUERY_OBJECTS` |
| `mod.json` | Mod metadata, `ws-port` setting (default 1314) |
| `logo.png` | Mod list icon |

### Adding a new action (checklist)

1. Implement `runMyAction(LevelEditorLayer*, nonce, request)` in `src/actions/MyAction.hpp`
2. Use `openEditorAccess()` and shared helpers from `EditorContext.hpp`
3. Add one line to `buildActionRegistry()` in `ActionRegistry.hpp`
4. **Do not** edit `WsServer.cpp` routing unless adding a new sync action like `PING`
5. Rebuild; test with wscat

## Implemented API (v1.0.0)

See [API.md](./API.md) for full request/response shapes.

| Action | Sync? | Requires editor? | Summary |
|--------|-------|------------------|---------|
| `PING` | yes | no | `{ modLoaded, inEditor }` |
| `GET_SELECTION` | no | yes | Current selection snapshot |
| `GET_LEVEL_SUMMARY` | no | yes | Object counts, groups, bounds |
| `QUERY_OBJECTS` | no | yes | Filter by objectId, group, rect, targetGroup |
| `SELECT_OBJECTS` | no | yes | Select by `uniqueId` / `uniqueIds` |
| `DESELECT_ALL` | no | yes | Clear selection |
| `CREATE_OBJECT` | no | yes | Place object by `objectId`, `x`, `y` |
| `MOVE_SELECTED_LEFT/RIGHT/UP/DOWN` | no | yes | One grid nudge via `EditCommand` |
| `NUDGE_SELECTED` | no | yes | `{ direction, step? }` flexible nudge |

### Removed / failed experiments

- **`SET_SELECTED_COLOR`** — removed. Blocks use **color channels**, not direct RGB; `colorSelectClosed` did not visibly change blocks. Do not re-add without channel-based approach.

## Testing

Prerequisites: GD running, mod enabled, **inside level editor** for editor-gated actions.

```bash
# Health check
npx -y wscat -c ws://127.0.0.1:1314 -w 5 -x '{"nonce":"1","action":"PING"}'

# Create basic block (objectId 1), select it
npx -y wscat -c ws://127.0.0.1:1314 -w 5 -x '{"nonce":"c","action":"CREATE_OBJECT","objectId":1,"x":150,"y":105}'

# Read selection
npx -y wscat -c ws://127.0.0.1:1314 -w 5 -x '{"nonce":"s","action":"GET_SELECTION"}'

# Level overview
npx -y wscat -c ws://127.0.0.1:1314 -w 5 -x '{"nonce":"sum","action":"GET_LEVEL_SUMMARY"}'

# Find cube blocks in an area
npx -y wscat -c ws://127.0.0.1:1314 -w 5 -x '{"nonce":"q","action":"QUERY_OBJECTS","objectId":1,"rect":{"minX":0,"minY":0,"maxX":600,"maxY":300}}'

# Select by uniqueId, then move
npx -y wscat -c ws://127.0.0.1:1314 -w 5 -x '{"nonce":"sel","action":"SELECT_OBJECTS","uniqueId":16}'
npx -y wscat -c ws://127.0.0.1:1314 -w 5 -x '{"nonce":"m","action":"MOVE_SELECTED_LEFT"}'
```

### Common errors

| errorCode | Meaning |
|-----------|---------|
| `NOT_IN_EDITOR` | Not in level editor — open Build tab in a level |
| `NO_SELECTION` | Move actions with nothing selected |
| `UNKNOWN_ACTION` | Typo or old mod still loaded — rebuild + restart GD |
| `OBJECT_NOT_FOUND` | `uniqueId` not in level |
| `CREATE_FAILED` | Invalid `objectId` for create |

### Logs

Geode logs (Mac Steam):

`~/Library/Application Support/Steam/steamapps/common/Geometry Dash/Geometry Dash.app/Contents/geode/logs/`

Search for `[EditorCursor]` or `Failed to load image` (mod icon issues).

## Geometry Dash domain model (essential)

### Everything is a GameObject

On disk, all level things share the same **level-string key/value format**. In memory, `m_objectID` picks the C++ subclass:

```
GameObject
 └── EnhancedGameObject
      └── EffectGameObject   ← most triggers, portals, pads
           └── CameraTriggerGameObject, CountTriggerGameObject, ...
```

There is **no separate “trigger layer”** in storage — triggers are objects with trigger object IDs.

### Groups, not direct references

Triggers affect objects via **group IDs**, not pointers:

- Blocks/triggers have **`m_groups`** (level key 57) — “I am in group N”
- Triggers have **`m_targetGroupID`** (level key 51) — “affect group N”

Example: move trigger (objectId **901**) with `targetGroup: 5` moves all objects in group 5.

### Useful editor APIs (Geode bindings)

| API | Use |
|-----|-----|
| `EditorUI::createObject(objectID, pos)` | Place object (used by CREATE_OBJECT) |
| `EditorUI::selectObject` / `selectObjects` | Selection |
| `EditorUI::moveObjectCall(EditCommand)` | Grid nudge (used by move actions) |
| `EditorUI::getGridSnappedPos` | Snap placement to grid |
| `LevelEditorLayer::findGameObject(uniqueID)` | Lookup for SELECT_OBJECTS |
| `LevelEditorLayer::getAllObjects()` | Future QUERY_OBJECTS |
| `EffectGameObject::m_targetGroupID`, `m_moveOffset`, `m_duration` | Trigger fields |

### Test object IDs

| objectId | Thing |
|----------|-------|
| 1 | Basic cube block (good for create/move/select tests) |
| 901 | Move trigger |
| 899 | Color trigger |

**Do not** test block color via RGB — use color **channels** (1–999).

### Raw `objectId` is dangerous (documented, not fixed yet)

The API accepts **bare numeric `objectId`** on `CREATE_OBJECT` with minimal validation. Risks:

- Clients can pass wrong/hallucinated IDs and place the wrong object type.
- **`objectId` (type) ≠ `uniqueId` (instance)** — always chain create → capture `uniqueId` → use that for select/move.
- IDs are **GD 2.2081–specific**; do not assume they transfer across versions.
- Mod lists `geode.node-ids` as a dependency but **does not yet** map names → IDs in the WebSocket API.

**Current policy:** keep numeric IDs in the API for now; warn in [API.md](./API.md#object-ids--two-kinds-use-with-care). Future work: named types, validation/allowlist, or `DESCRIBE_OBJECT` / catalog endpoint before blind creates.

## Roadmap (not implemented)

Priority order discussed with user:

### Phase 1 — Read context (partially done)

- [x] `GET_SELECTION`
- [x] `QUERY_OBJECTS` — filter by rect, objectId, group, targetGroup
- [x] `GET_LEVEL_SUMMARY` — counts, group inventory
- [ ] `DESCRIBE_OBJECT` — full field map for one `uniqueId`

### Phase 2 — Groups (needed before triggers work end-to-end)

- [ ] `SET_OBJECT_GROUPS` — assign block to group N
- [ ] `GET_GROUP_MEMBERS` — reverse lookup
- Use `LevelEditorLayer::getNextFreeGroupID()` for auto group assignment

### Phase 3 — Triggers

- [ ] `CREATE_OBJECT` with trigger IDs (901, etc.) — create already works generically
- [ ] `SET_TRIGGER_TARGET_GROUP`
- [ ] `SET_MOVE_TRIGGER` — offset, duration, easing on `EffectGameObject`
- [ ] `GET_TRIGGER_GRAPH` — groups + triggers + members

Start with **move trigger only**; color triggers need channel semantics.

### Phase 4 — Polish

- [ ] README for end users (root `README.md` is just a pointer)
- [ ] Undo integration audit (`createObject` vs `LevelEditorLayer::createObject(..., noUndo)`)
- [ ] Optional `QUERY_OBJECTS` + Cursor context pipeline

## Known gotchas for future agents

1. **Restart GD after build** — UNKNOWN_ACTION often means stale mod in memory.
2. **Queue must drain on main thread** — use `postUpdate`, not `schedule_selector`.
3. **Async WS responses** — most actions queue; wscat needs `-w 5` or higher. Empty response = queue not draining or connection closed early.
4. **matjson** — use `isNumber()` / `isBool()` / `isArray()`; there is no `isInt()` / `isDouble()`.
5. **Object handles** — always use `uniqueId` (session-stable in editor), not array index.
6. **Mod icon** — must be `logo.png` at repo root; 32×32 failed to display well; 256×256 works.
7. **Geode dependency** — `geode.node-ids` required in `mod.json`.
8. **User rules** — Postgres/Sequelize/ES6 rules in Cursor are for Node projects; **ignore for this C++ Geode mod**.

## Dependencies

- Geode SDK 5.7.1, GD 2.2081
- `geode.node-ids` (mod dependency)
- websocketpp + asio (fetched by CMake CPMAddPackage)
- C++23, CMake 3.21+

## Git / commits

User prefers **not** to commit unless explicitly asked. Do not auto-commit.
