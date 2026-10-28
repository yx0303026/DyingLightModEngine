#pragma once

#include "EngineAbi.h"

#include <Windows.h>

#include <cstdint>

/*
 * Games built on DirectInput (Dying Light reads the mouse through
 * IDirectInputDevice8::GetDeviceState) never consult window messages or the system
 * cursor, so filtering WM_INPUT/WM_MOUSEMOVE cannot stop them from steering the
 * camera. This runtime hooks the DirectInput mouse device, suppresses its state
 * while a mod UI is open, and republishes the relative motion as a virtual cursor
 * that mods feed into their ImGui backend.
 */
class EngineDirectInputRuntime final {
public:
	EngineDirectInputRuntime() = delete;

	/* Installs the DirectInput hooks. Returns false when no mouse device could be hooked. */
	static bool Install() noexcept;
	static void Uninstall() noexcept;
	static void SetGameWindow(HWND window) noexcept;
	static void SetUiActive(bool active) noexcept;
	static bool IsMouseCaptured() noexcept;

	/* Host API entry point used by mods through DlModHostApiV1::getUiMouseState. */
	static uint8_t MODSDK_CALL GetUiMouseState(DlModUiMouseStateV1* state) noexcept;
};
