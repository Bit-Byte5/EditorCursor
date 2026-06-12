# EditorCursor

Geode mod that exposes a localhost WebSocket API for controlling the Geometry Dash level editor from external tools (e.g. Cursor IDE).

**Mod ID:** `paxcirlot.editorcursor` · **Default port:** `1314` · **GD 2.2081** · **Geode 5.7.1**

## Quick start

```bash
GEODE_SDK=/Users/Shared/Geode/sdk geode build
```

Restart Geometry Dash with the mod enabled. Open the level editor, then:

```bash
npx -y wscat -c ws://127.0.0.1:1314 -w 5 -x '{"nonce":"1","action":"PING"}'
```

## Documentation

| Doc | Contents |
|-----|----------|
| [docs/HANDOFF.md](docs/HANDOFF.md) | Architecture, build, gotchas, GD domain model, roadmap — **start here for agent continuation** |
| [docs/API.md](docs/API.md) | Full WebSocket protocol reference |
