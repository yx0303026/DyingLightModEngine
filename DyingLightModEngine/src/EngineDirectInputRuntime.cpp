#include "EngineDirectInputRuntime.h"

#include "EngineLogger.h"

#include <MinHook.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <vector>

namespace {

	constexpr std::size_t kMaxHookedTargets = 8;
	constexpr DWORD kMouseStateSize = static_cast<DWORD>(sizeof(DIMOUSESTATE));
	constexpr DWORD kMouseState2Size = static_cast<DWORD>(sizeof(DIMOUSESTATE2));

	using DirectInput8CreateFn = HRESULT(__stdcall*)(HINSTANCE, DWORD, const IID&, void**, void*);
	using CreateDeviceFn = HRESULT(__stdcall*)(void*, const GUID&, void**, void*);
	using GetDeviceStateFn = HRESULT(__stdcall*)(void*, DWORD, void*);
	using GetDeviceDataFn = HRESULT(__stdcall*)(void*, DWORD, DIDEVICEOBJECTDATA*, DWORD*, DWORD);

	std::atomic_bool g_installed = false;
	std::atomic_bool g_uiActive = false;
	std::atomic_bool g_mouseCaptured = false;
	std::atomic<HWND> g_gameWindow = nullptr;
	std::atomic<int32_t> g_clientWidth = 0;
	std::atomic<int32_t> g_clientHeight = 0;
	std::atomic<int32_t> g_cursorX = 0;
	std::atomic<int32_t> g_cursorY = 0;
	std::atomic<uint32_t> g_buttons = 0;
	std::atomic<int32_t> g_wheelTicks = 0;

	GetDeviceStateFn g_originalGetDeviceState = nullptr;
	GetDeviceDataFn g_originalGetDeviceData = nullptr;
	CreateDeviceFn g_originalCreateDevice = nullptr;
	std::array<void*, kMaxHookedTargets> g_hookedTargets{};
	std::size_t g_hookedTargetCount = 0;

	bool EnsureHookRuntime() noexcept
	{
		static const MH_STATUS status = MH_Initialize();
		return status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED;
	}

	void UpdateClientSize() noexcept
	{
		const HWND window = g_gameWindow.load(std::memory_order_relaxed);
		if (window == nullptr) {
			return;
		}
		RECT client{};
		if (GetClientRect(window, &client) == FALSE) {
			return;
		}
		g_clientWidth.store(client.right > 0 ? client.right : 0, std::memory_order_relaxed);
		g_clientHeight.store(client.bottom > 0 ? client.bottom : 0, std::memory_order_relaxed);
	}

	int32_t ClampToRange(int32_t value, int32_t extent) noexcept
	{
		const int32_t maximum = extent > 0 ? extent - 1 : 0;
		if (value < 0) {
			return 0;
		}
		return value > maximum ? maximum : value;
	}

	void MoveCursor(int32_t deltaX, int32_t deltaY) noexcept
	{
		const int32_t width = g_clientWidth.load(std::memory_order_relaxed);
		const int32_t height = g_clientHeight.load(std::memory_order_relaxed);
		const int32_t x = ClampToRange(g_cursorX.load(std::memory_order_relaxed) + deltaX, width);
		const int32_t y = ClampToRange(g_cursorY.load(std::memory_order_relaxed) + deltaY, height);
		g_cursorX.store(x, std::memory_order_relaxed);
		g_cursorY.store(y, std::memory_order_relaxed);
	}

	void SetButtonState(uint32_t button, bool down) noexcept
	{
		if (button >= 8U) {
			return;
		}
		const uint32_t mask = 1U << button;
		uint32_t current = g_buttons.load(std::memory_order_relaxed);
		for (;;) {
			const uint32_t updated = down ? (current | mask) : (current & ~mask);
			if (g_buttons.compare_exchange_weak(current, updated, std::memory_order_relaxed)) {
				return;
			}
		}
	}

	void AccumulateMouseState(const DIMOUSESTATE* state, DWORD size) noexcept
	{
		uint32_t buttons = 0;
		const DWORD buttonCount = size >= kMouseState2Size ? 8U : 4U;
		for (DWORD index = 0; index < buttonCount; ++index) {
			if ((state->rgbButtons[index] & 0x80U) != 0) {
				buttons |= (1U << index);
			}
		}
		g_buttons.store(buttons, std::memory_order_relaxed);
		if (state->lZ != 0) {
			g_wheelTicks.fetch_add(state->lZ, std::memory_order_relaxed);
		}
		if (state->lX != 0 || state->lY != 0) {
			MoveCursor(state->lX, state->lY);
		}
	}

	void AccumulateRawMouseState(const RAWMOUSE& mouse) noexcept
	{
		if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0U) {
			const int32_t width = g_clientWidth.load(std::memory_order_relaxed);
			const int32_t height = g_clientHeight.load(std::memory_order_relaxed);
			if (width > 0 && height > 0) {
				g_cursorX.store(
					static_cast<int32_t>((static_cast<uint64_t>(mouse.lLastX) *
						static_cast<uint64_t>(width - 1)) / 65535ULL),
					std::memory_order_relaxed);
				g_cursorY.store(
					static_cast<int32_t>((static_cast<uint64_t>(mouse.lLastY) *
						static_cast<uint64_t>(height - 1)) / 65535ULL),
					std::memory_order_relaxed);
			}
		}
		else if (mouse.lLastX != 0 || mouse.lLastY != 0) {
			MoveCursor(mouse.lLastX, mouse.lLastY);
		}

		const USHORT buttonFlags = static_cast<USHORT>(mouse.ulButtons);
		if ((buttonFlags & RI_MOUSE_BUTTON_1_DOWN) != 0U) {
			SetButtonState(0U, true);
		}
		if ((buttonFlags & RI_MOUSE_BUTTON_1_UP) != 0U) {
			SetButtonState(0U, false);
		}
		if ((buttonFlags & RI_MOUSE_BUTTON_2_DOWN) != 0U) {
			SetButtonState(1U, true);
		}
		if ((buttonFlags & RI_MOUSE_BUTTON_2_UP) != 0U) {
			SetButtonState(1U, false);
		}
		if ((buttonFlags & RI_MOUSE_BUTTON_3_DOWN) != 0U) {
			SetButtonState(2U, true);
		}
		if ((buttonFlags & RI_MOUSE_BUTTON_3_UP) != 0U) {
			SetButtonState(2U, false);
		}
		if ((buttonFlags & RI_MOUSE_BUTTON_4_DOWN) != 0U) {
			SetButtonState(3U, true);
		}
		if ((buttonFlags & RI_MOUSE_BUTTON_4_UP) != 0U) {
			SetButtonState(3U, false);
		}
		if ((buttonFlags & RI_MOUSE_BUTTON_5_DOWN) != 0U) {
			SetButtonState(4U, true);
		}
		if ((buttonFlags & RI_MOUSE_BUTTON_5_UP) != 0U) {
			SetButtonState(4U, false);
		}
		if ((buttonFlags & RI_MOUSE_WHEEL) != 0U) {
			g_wheelTicks.fetch_add(
				static_cast<SHORT>(mouse.usButtonData),
				std::memory_order_relaxed);
		}
	}

	bool HookTarget(void* target, void* detour, void** original) noexcept
	{
		if (target == nullptr || detour == nullptr || original == nullptr) {
			return false;
		}
		for (std::size_t index = 0; index < g_hookedTargetCount; ++index) {
			if (g_hookedTargets[index] == target) {
				return true;
			}
		}
		if (g_hookedTargetCount >= g_hookedTargets.size()) {
			return false;
		}
		if (MH_CreateHook(target, detour, original) != MH_OK) {
			return false;
		}
		if (MH_EnableHook(target) != MH_OK) {
			MH_RemoveHook(target);
			return false;
		}
		g_hookedTargets[g_hookedTargetCount++] = target;
		return true;
	}

	HRESULT __stdcall GetDeviceStateDetour(void* device, DWORD size, void* data) noexcept
	{
		const HRESULT result = g_originalGetDeviceState != nullptr
			? g_originalGetDeviceState(device, size, data)
			: E_FAIL;
		if (FAILED(result) || data == nullptr || !g_uiActive.load(std::memory_order_relaxed)) {
			return result;
		}
		if (size != kMouseStateSize && size != kMouseState2Size) {
			return result;
		}
		AccumulateMouseState(static_cast<const DIMOUSESTATE*>(data), size);
		SecureZeroMemory(data, size);
		return result;
	}

	HRESULT __stdcall GetDeviceDataDetour(
		void* device,
		DWORD objectSize,
		DIDEVICEOBJECTDATA* data,
		DWORD* count,
		DWORD flags) noexcept
	{
		const HRESULT result = g_originalGetDeviceData != nullptr
			? g_originalGetDeviceData(device, objectSize, data, count, flags)
			: E_FAIL;
		if (FAILED(result) || data == nullptr || count == nullptr ||
			!g_uiActive.load(std::memory_order_relaxed)) {
			return result;
		}
		const DWORD available = *count;
		for (DWORD index = 0; index < available; ++index) {
			const DWORD offset = data[index].dwOfs;
			if (offset == DIMOFS_X) {
				MoveCursor(static_cast<LONG>(data[index].dwData), 0);
			}
			else if (offset == DIMOFS_Y) {
				MoveCursor(0, static_cast<LONG>(data[index].dwData));
			}
			else if (offset == DIMOFS_Z) {
				g_wheelTicks.fetch_add(static_cast<LONG>(data[index].dwData), std::memory_order_relaxed);
			}
			else if (offset >= DIMOFS_BUTTON0 && offset <= DIMOFS_BUTTON7) {
				SetButtonState(offset - DIMOFS_BUTTON0, (data[index].dwData & 0x80U) != 0);
			}
		}
		*count = 0;
		return result;
	}

	void HookMouseDevice(void* device) noexcept
	{
		if (device == nullptr) {
			return;
		}
		auto* typed = static_cast<IDirectInputDevice8W*>(device);
		DIDEVCAPS capabilities{};
		capabilities.dwSize = sizeof(capabilities);
		if (FAILED(typed->GetCapabilities(&capabilities)) ||
			GET_DIDEVICE_TYPE(capabilities.dwDevType) != DI8DEVTYPE_MOUSE) {
			return;
		}
		void** vtable = *static_cast<void***>(device);
		if (vtable == nullptr) {
			return;
		}
		const bool stateHooked = HookTarget(
			vtable[9],
			reinterpret_cast<void*>(&GetDeviceStateDetour),
			reinterpret_cast<void**>(&g_originalGetDeviceState));
		const bool dataHooked = HookTarget(
			vtable[10],
			reinterpret_cast<void*>(&GetDeviceDataDetour),
			reinterpret_cast<void**>(&g_originalGetDeviceData));
		if (stateHooked || dataHooked) {
			g_mouseCaptured.store(true, std::memory_order_relaxed);
		}
	}

	HRESULT __stdcall CreateDeviceDetour(
		void* directInput,
		const GUID& deviceGuid,
		void** device,
		void* outer) noexcept
	{
		if (g_originalCreateDevice == nullptr) {
			return E_FAIL;
		}
		const HRESULT result = g_originalCreateDevice(directInput, deviceGuid, device, outer);
		if (SUCCEEDED(result) && device != nullptr && *device != nullptr) {
			HookMouseDevice(*device);
		}
		return result;
	}

	void HookDirectInputInterface(DirectInput8CreateFn create, const IID& interfaceId) noexcept
	{
		if (create == nullptr) {
			return;
		}
		void* directInput = nullptr;
		if (FAILED(create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, interfaceId, &directInput, nullptr)) ||
			directInput == nullptr) {
			return;
		}
		void** vtable = *static_cast<void***>(directInput);
		if (vtable != nullptr) {
			HookTarget(
				vtable[3],
				reinterpret_cast<void*>(&CreateDeviceDetour),
				reinterpret_cast<void**>(&g_originalCreateDevice));
			/* Capture the mouse device vtable before the game creates its own device. */
			void* device = nullptr;
			if (g_originalCreateDevice != nullptr &&
				SUCCEEDED(g_originalCreateDevice(directInput, GUID_SysMouse, &device, nullptr)) &&
				device != nullptr) {
				HookMouseDevice(device);
				static_cast<IUnknown*>(device)->Release();
			}
		}
		static_cast<IUnknown*>(directInput)->Release();
	}

} // namespace

bool EngineDirectInputRuntime::Install() noexcept
{
	if (g_installed.exchange(true)) {
		return g_mouseCaptured.load(std::memory_order_relaxed);
	}
	if (!EnsureHookRuntime()) {
		g_installed.store(false, std::memory_order_relaxed);
		return false;
	}
	HMODULE module = GetModuleHandleW(L"dinput8.dll");
	if (module == nullptr) {
		module = LoadLibraryW(L"dinput8.dll");
	}
	if (module == nullptr) {
		g_installed.store(false, std::memory_order_relaxed);
		return false;
	}
	const auto create = reinterpret_cast<DirectInput8CreateFn>(
		GetProcAddress(module, "DirectInput8Create"));
	if (create == nullptr) {
		g_installed.store(false, std::memory_order_relaxed);
		return false;
	}
	HookDirectInputInterface(create, IID_IDirectInput8W);
	HookDirectInputInterface(create, IID_IDirectInput8A);
	const bool captured = g_mouseCaptured.load(std::memory_order_relaxed);
	if (captured) {
		EngineLogger::Info("DirectInput mouse device hooked; mod UI receives the pointer while it is open.");
	}
	else {
		EngineLogger::Warning("No DirectInput mouse device vtable could be hooked; game mouse input may not be blocked.");
		g_installed.store(false, std::memory_order_relaxed);
	}
	return captured;
}

void EngineDirectInputRuntime::Uninstall() noexcept
{
	for (std::size_t index = 0; index < g_hookedTargetCount; ++index) {
		MH_DisableHook(g_hookedTargets[index]);
		MH_RemoveHook(g_hookedTargets[index]);
	}
	g_hookedTargetCount = 0;
	g_originalGetDeviceState = nullptr;
	g_originalGetDeviceData = nullptr;
	g_originalCreateDevice = nullptr;
	g_mouseCaptured.store(false, std::memory_order_relaxed);
	g_uiActive.store(false, std::memory_order_relaxed);
	g_installed.store(false, std::memory_order_relaxed);
}

void EngineDirectInputRuntime::SetGameWindow(HWND window) noexcept
{
	g_gameWindow.store(window, std::memory_order_relaxed);
	UpdateClientSize();
	const int32_t width = g_clientWidth.load(std::memory_order_relaxed);
	const int32_t height = g_clientHeight.load(std::memory_order_relaxed);
	g_cursorX.store(width / 2, std::memory_order_relaxed);
	g_cursorY.store(height / 2, std::memory_order_relaxed);
}

void EngineDirectInputRuntime::SetUiActive(bool active) noexcept
{
	const bool previous = g_uiActive.exchange(active, std::memory_order_relaxed);
	if (previous == active) {
		return;
	}
	if (!active) {
		EngineLogger::Info("Mod UI closed; DirectInput mouse state is forwarded to the game again.");
		return;
	}
	UpdateClientSize();
	const int32_t width = g_clientWidth.load(std::memory_order_relaxed);
	const int32_t height = g_clientHeight.load(std::memory_order_relaxed);
	g_cursorX.store(width / 2, std::memory_order_relaxed);
	g_cursorY.store(height / 2, std::memory_order_relaxed);
	g_buttons.store(0, std::memory_order_relaxed);
	g_wheelTicks.store(0, std::memory_order_relaxed);
	EngineLogger::Info("Mod UI opened; DirectInput mouse state is routed to the UI.");
}

bool EngineDirectInputRuntime::IsMouseCaptured() noexcept
{
	return g_mouseCaptured.load(std::memory_order_relaxed);
}

void EngineDirectInputRuntime::HandleRawInput(HRAWINPUT input) noexcept
{
	if (input == nullptr || !g_uiActive.load(std::memory_order_relaxed)) {
		return;
	}

	UINT size = 0;
	if (GetRawInputData(input, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) ==
		static_cast<UINT>(-1) || size < sizeof(RAWINPUTHEADER)) {
		return;
	}

	std::vector<std::byte> buffer;
	try {
		buffer.resize(size);
	}
	catch (...) {
		return;
	}

	if (GetRawInputData(
		input,
		RID_INPUT,
		buffer.data(),
		&size,
		sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1) ||
		size < sizeof(RAWINPUT)) {
		return;
	}

	const auto* raw = reinterpret_cast<const RAWINPUT*>(buffer.data());
	if (raw->header.dwType == RIM_TYPEMOUSE) {
		AccumulateRawMouseState(raw->data.mouse);
	}
}

uint8_t MODSDK_CALL EngineDirectInputRuntime::GetUiMouseState(DlModUiMouseStateV1* state) noexcept
{
	if (state == nullptr || state->structSize < sizeof(DlModUiMouseStateV1)) {
		return 0U;
	}
	state->abiVersion = MODSDK_ABI_VERSION;
	state->flags = 0U;
	state->positionX = 0;
	state->positionY = 0;
	state->wheelDelta = 0.0F;
	state->buttons = 0U;
	if (!g_uiActive.load(std::memory_order_relaxed)) {
		return 0U;
	}
	UpdateClientSize();
	state->flags = DLMOD_UI_MOUSE_POSITION_VALID;
	if (g_mouseCaptured.load(std::memory_order_relaxed)) {
		state->flags |= DLMOD_UI_MOUSE_DEVICE_CAPTURED;
	}
	state->positionX = g_cursorX.load(std::memory_order_relaxed);
	state->positionY = g_cursorY.load(std::memory_order_relaxed);
	state->buttons = g_buttons.load(std::memory_order_relaxed);
	const int32_t ticks = g_wheelTicks.exchange(0, std::memory_order_relaxed);
	state->wheelDelta = static_cast<float>(ticks) / static_cast<float>(WHEEL_DELTA);
	return 1U;
}
