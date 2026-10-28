# Cheat Mod

`CheatMod` is a loadable Dying Light Mod Engine module that builds its menu
with `DyingLightModEngine::ImGui`. The menu has a vertical navigation sidebar
and a page content area containing Dashboard, Player, and Settings pages.

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
