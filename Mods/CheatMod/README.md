# Cheat Mod

`CheatMod` is a loadable Dying Light Mod Engine module that builds its menu
with `DyingLightModEngine::ImGui`. The menu has a vertical navigation sidebar
and a page content area containing Dashboard, Player, Bullet Tracking, Room List,
Room Members, and Settings pages.

The Bullet Tracking page implements projectile guidance entirely inside the
module: it registers a frame callback and a projectile update callback through
`DlModProjectileApiV1`, locks the entity closest to the crosshair, and rewrites
the BulletManager velocity before the native update runs. Arrow, bolt and bullet
records are classified by the engine from their visual RTTI, so bows, crossbows,
guns and tracers share one path. Tracking only runs while a single-player
session is active; the engine never changes projectile state on its own.

Target acquisition goes through `DlModEntityApiV1::pickAimTarget`: one engine
call enumerates actors once, caches that actor list for 200 ms, projects every
candidate with a single camera matrix read, re-projects only the few candidates
nearest to the crosshair with their cached bone id, and returns the winner plus
its bone id. Per-frame tracking then uses `readBoneById`, so no bone name lookup
runs after the first frame. Hosts that do not expose the extended entity table
fall back to the legacy `enumerateNearby` / `readBoneByName` /
`DlModCameraApiV1::project` path, which is selected automatically.


The Room List page shows room discovery results, while Room Members owns the
current-member and history views plus clipboard/history actions. Both pages
consume the Engine Lobby snapshot API. Lobby discovery and
EOS data collection stay inside the Engine runtime; the mod only receives a
stable, copied snapshot through `DlModLobbyApiV1` and renders it with the Engine
ImGui facade.

Pressing F2 toggles the menu. The module polls F2 and exposes
`CheatMod_HandleKeyEvent` for hosts that provide their own input hook. The
engine automatically initializes the Win32/DX11 ImGui backend and invokes the
module's render callback from the game's DX11 `Present` path.

While the menu is open, the engine hides the system cursor, renders ImGui's
cursor, captures the game window, and consumes keyboard, mouse, and raw-input
messages so gameplay controls such as camera movement do not receive them.

`CheatMod_RenderFrame` remains available for ABI compatibility; normal engine
integration uses the descriptor render callback.

Build the module with:

```text
cmake --build out/build-vs2026/x64-release --config Release --target CheatMod
```
