#pragma once

#include "sdk/EngineAbi.h"

#include <Windows.h>

#include <cstdint>

/*
 * Games built on DirectInput (Dying Light reads the mouse through
 * IDirectInputDevice8::GetDeviceState) normally never consult window messages or
 * the system cursor. This runtime hooks the DirectInput mouse device when
 * possible, and also hooks GetRawInputData/GetRawInputBuffer plus accepts
 * WM_INPUT samples as a fallback. While a mod UI is open, native mouse state
 * is suppressed and relative motion is republished as a virtual cursor that
 * mods feed into their ImGui backend.
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
	static void HandleRawInput(HRAWINPUT input) noexcept;

	/* Host API entry point used by mods through DlModHostApiV1::getUiMouseState. */
	static uint8_t MODSDK_CALL GetUiMouseState(DlModUiMouseStateV1* state) noexcept;
};
