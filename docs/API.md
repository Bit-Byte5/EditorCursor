# EditorCursor WebSocket API

**Endpoint:** `ws://127.0.0.1:1314` (configurable via Geode mod setting `ws-port`)

**Binding:** `127.0.0.1` only — not exposed on LAN.

## Request format

All messages are JSON objects:

```json
{
  "nonce": "unique-string-per-request",
  "action": "ACTION_NAME",
  "...actionSpecificFields": "..."
}
```

- `nonce` — required, non-empty string (echoed in response)
- `action` — required string

## Object IDs — two kinds, use with care

The API currently uses **raw numeric IDs** only. There is no named-type layer yet.

| Field | Scope | Used in | Meaning |
|-------|-------|---------|---------|
| `objectId` | Object **type** | `CREATE_OBJECT`, snapshots | Which GD object to place (e.g. `1` = cube block, `901` = move trigger) |
| `uniqueId` | Object **instance** | `SELECT_OBJECTS`, snapshots | Session-stable ID of one placed object in the level |

### Why raw `objectId` is risky

- **Opaque numbers** — easy for clients (especially LLMs) to guess wrong IDs, place the wrong thing, or confuse triggers with blocks.
- **No friendly names in the API** — callers must know GD’s numeric catalog; the mod depends on `geode.node-ids` but does not expose named lookups yet.
- **Weak validation** — we only check that `objectId` is a positive integer; invalid IDs fail at `CREATE_FAILED`, but some wrong IDs may still create unexpected objects.
- **Version drift** — object ID meanings are tied to GD **2.2081**; other versions or custom objects may differ.
- **Type vs instance** — mixing up `objectId` (type) and `uniqueId` (instance) causes subtle bugs (e.g. selecting type `1` instead of the block you just created).

### Current policy

**Keep using numeric `objectId` for now** — no API change planned yet. Treat docs and test tables as the source of truth; prefer `uniqueId` from `CREATE_OBJECT` / `GET_SELECTION` for all follow-up actions. A future layer may add named types (via `geode.node-ids`) or an allowlist before create.

## Response format

See [HANDOFF.md](./HANDOFF.md#response-envelope).

---

## PING

Health check. Runs synchronously on the WebSocket thread.

**Request:**

```json
{ "nonce": "1", "action": "PING" }
```

**Response `response`:**

```json
{
  "modLoaded": true,
  "inEditor": true
}
```

---

## GET_SELECTION

Read current editor selection.

**Request:**

```json
{ "nonce": "1", "action": "GET_SELECTION" }
```

**Response `response`:**

```json
{
  "selectedCount": 1,
  "objects": [
    { "uniqueId": 16, "objectId": 1, "x": 150, "y": 105 }
  ]
}
```

---

## SELECT_OBJECTS

Select objects by level unique IDs.

**Request (single):**

```json
{ "nonce": "1", "action": "SELECT_OBJECTS", "uniqueId": 16 }
```

**Request (multiple):**

```json
{
  "nonce": "1",
  "action": "SELECT_OBJECTS",
  "uniqueIds": [16, 17, 18],
  "additive": false,
  "ignoreFilter": false
}
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `uniqueId` | number | — | Single ID (use this or `uniqueIds`) |
| `uniqueIds` | number[] | — | Multiple IDs |
| `additive` | bool | `false` | If true, add to existing selection |
| `ignoreFilter` | bool | `false` | Passed to editor select calls |

**Response `response`:**

```json
{
  "selectedCount": 2,
  "objects": [ ... ],
  "missingUniqueIds": [99]
}
```

`missingUniqueIds` only present when some IDs were not found but at least one succeeded.

**Errors:** `MISSING_UNIQUE_IDS`, `OBJECT_NOT_FOUND`

---

## GET_LEVEL_SUMMARY

Overview of everything in the open level. Useful for giving Cursor situational awareness before editing.

**Request:**

```json
{ "nonce": "1", "action": "GET_LEVEL_SUMMARY" }
```

**Response `response`:**

```json
{
  "totalObjectCount": 842,
  "selectedCount": 0,
  "triggerCount": 12,
  "uniqueObjectIdTypes": 5,
  "uniqueGroupCount": 3,
  "objectIdCounts": { "1": 800, "901": 12 },
  "groupIds": [1, 5, 12],
  "bounds": { "minX": 30, "minY": 105, "maxX": 1920, "maxY": 300 }
}
```

`bounds` is omitted when the level has no objects.

---

## QUERY_OBJECTS

Find objects matching one or more filters. At least one filter is required.

**Request:**

```json
{
  "nonce": "1",
  "action": "QUERY_OBJECTS",
  "objectId": 1,
  "groupId": 5,
  "targetGroupId": 5,
  "rect": { "minX": 0, "minY": 0, "maxX": 300, "maxY": 300 },
  "limit": 50
}
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `objectId` | number | — | Match GD object type ID |
| `groupId` | number | — | Object belongs to this group |
| `targetGroupId` | number | — | Trigger targets this group |
| `rect` | object | — | Axis-aligned bounds (`minX`, `minY`, `maxX`, `maxY`) |
| `limit` | number | `50` | Max objects returned (cap `200`) |

Filters combine with **AND** logic.

**Response `response`:**

```json
{
  "matchCount": 3,
  "returnedCount": 3,
  "truncated": false,
  "limit": 50,
  "objects": [
    {
      "uniqueId": 16,
      "objectId": 1,
      "x": 150,
      "y": 105,
      "groups": [5],
      "isTrigger": false
    }
  ]
}
```

Triggers may include `targetGroupId`. `groups` is omitted when empty.

**Errors:** `MISSING_FILTER`, `INVALID_RECT`, `INVALID_LIMIT`

---

## DESELECT_ALL

Clear selection.

**Request:**

```json
{ "nonce": "1", "action": "DESELECT_ALL" }
```

**Response `response`:**

```json
{ "selectedCount": 0 }
```

---

## CREATE_OBJECT

Place a new object in the level.

**Request:**

```json
{
  "nonce": "1",
  "action": "CREATE_OBJECT",
  "objectId": 1,
  "x": 150,
  "y": 105,
  "snapToGrid": true,
  "select": true
}
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `objectId` | number | required | GD object type ID (also accepts `objectID`). See [Object IDs](#object-ids--two-kinds-use-with-care). |
| `x`, `y` | number | required | Position |
| `snapToGrid` | bool | `true` | Snap via `EditorUI::getGridSnappedPos` |
| `select` | bool | `true` | Select object after create |

**Response `response`:**

```json
{
  "object": { "uniqueId": 17, "objectId": 1, "x": 150, "y": 105 }
}
```

**Errors:** `MISSING_OBJECT_ID`, `INVALID_OBJECT_ID`, `MISSING_POSITION`, `CREATE_FAILED`

---

## MOVE_SELECTED_LEFT / RIGHT / UP / DOWN

Nudge all selected objects one grid step.

**Request:**

```json
{ "nonce": "1", "action": "MOVE_SELECTED_LEFT" }
```

**Response `response`:**

```json
{
  "selectedCount": 1,
  "offset": { "x": -30, "y": 0 },
  "objects": [
    { "uniqueId": 16, "objectId": 1, "x": 120, "y": 105 }
  ]
}
```

**Errors:** `NO_SELECTION`

Uses `EditorUI::moveObjectCall(EditCommand::Left|Right|Up|Down)`.

---

## NUDGE_SELECTED

Flexible nudge with direction and step size.

**Request:**

```json
{
  "nonce": "1",
  "action": "NUDGE_SELECTED",
  "direction": "left",
  "step": "normal"
}
```

| Field | Values |
|-------|--------|
| `direction` | `left`, `right`, `up`, `down` |
| `step` (optional) | `normal` (default), `small`, `big`, `tiny` |

**Response:** Same shape as move actions.

**Errors:** `MISSING_DIRECTION`, `INVALID_DIRECTION`, `NO_SELECTION`

---

## Global errors

| errorCode | stage | When |
|-----------|-------|------|
| `NOT_IN_EDITOR` | `editor_gate` | Editor action outside level editor |
| `UNKNOWN_ACTION` | `parse` | Unrecognized `action` string |
| `MISSING_NONCE` | `parse` | Bad request shape |
| `INVALID_JSON` | `parse` | Malformed JSON |
| `INTERNAL_ERROR` | various | Unexpected exception |
| `EDITOR_LAYER_LOST` | `execute` | Editor layer null during drain |
| `EDITOR_UI_MISSING` | `execute` | EditorUI null |

---

## Typical client workflow

```
PING → confirm inEditor
GET_LEVEL_SUMMARY → understand level state
QUERY_OBJECTS → find objects by type/group/area
CREATE_OBJECT → capture uniqueId from response
SELECT_OBJECTS → re-select later
GET_SELECTION → verify state
MOVE_SELECTED_* / NUDGE_SELECTED → mutate selection
DESELECT_ALL → clear
```

All editor actions except `PING` are **queued** and execute on the next editor frame (~one frame latency). Keep the WebSocket connection open until the response arrives.
