#include "runtime/EngineHotkeyRuntime.h"

#include "logging/EngineLogger.h"
#include "runtime/EngineEntityBindings.h"

#include <MinHook.h>
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {

	constexpr wchar_t kGameModuleName[] = L"gamedll_x64_rwdi.dll";

	/* gamedll_x64_rwdi.dll（31,792,192 字节，MD5 97046bba...）中的静态地址。 */
	constexpr std::uint32_t kConverterSingletonRva = 0x1D02B20;
	constexpr std::uint32_t kReloadInputsRva = 0x11F14D0;
	constexpr std::uint32_t kCompileLayoutsRva = 0x11F12A0;
	constexpr std::uint32_t kCopyConverterStateRva = 0x11566F0;
	constexpr std::uint32_t kFindEntryByNameRva = 0x11F1D80;
	constexpr std::uint32_t kInsertTreeNodeRva = 0x12D6F50;
	constexpr std::uint32_t kDescriptorTableRva = 0x1CDE7D0;
	constexpr std::uint32_t kAddActionVtableRva = 0x1891C50;
	constexpr std::uint32_t kKeybindingVtableRva = 0x1891C20;
	constexpr std::uint32_t kInputsLayoutVtableRva = 0x1891BD8;

	constexpr std::uint32_t kConverterStateOffset = 0xC0;
	constexpr std::uint32_t kControllerStride = 0x40;
	constexpr std::uint32_t kControllerEntryOffset = 0x10;
	constexpr std::uint32_t kControllerEntryCountOffset = 0x18;
	constexpr std::uint32_t kControllerEntryCapacityOffset = 0x1C;
	constexpr std::uint32_t kControllerViewOffset = 0x28;
	constexpr std::uint32_t kControllerViewCountOffset = 0x30;
	constexpr std::uint32_t kControllerViewCapacityOffset = 0x34;
	constexpr std::uint32_t kPerEntrySelectionBase = 0xC8;
	constexpr std::uint32_t kPerEntrySelectionStride = 0x18;

	constexpr std::uint32_t kLayoutPresetOffset = 0x18;
	constexpr std::uint32_t kLayoutPresetCountOffset = 0x20;
	constexpr std::uint32_t kPresetOperationOffset = 0x10;
	constexpr std::uint32_t kPresetOperationCountOffset = 0x18;
	constexpr std::uint32_t kPresetOperationCapacityOffset = 0x1C;

	constexpr std::uint32_t kKeybindingTreeOffset = 0x48;
	constexpr std::uint32_t kKeybindingSize = 0x60;
	constexpr std::uint32_t kViewSize = 0x40;
	constexpr std::uint32_t kViewRowOffset = 0x10;
	constexpr std::uint32_t kViewRowCountOffset = 0x18;
	constexpr std::uint32_t kViewRowCapacityOffset = 0x1C;
	constexpr std::uint32_t kOperationSize = 0x38;
	constexpr std::uint32_t kTreeNodeSize = 0x28;
	constexpr std::uint32_t kViewRowSize = 0x18;
	constexpr std::uint32_t kConverterStateSize = 96;

	constexpr int kCategoryBase = 1;
	constexpr int kCollideBase = 1;
	constexpr int kMaxActionId = 337;
	constexpr int kDescriptorCount = 0x111;
	constexpr std::uint32_t kKeyboardDevice = 1;
	constexpr std::uint32_t kDefaultTarget = 2; /* EInputTarget_Module */

	constexpr std::uint64_t kBindingRefreshMilliseconds = 250;
	constexpr std::uint64_t kReloadDebounceMilliseconds = 1000;

	constexpr std::uint32_t kInvalidAction = DLMOD_HOTKEY_INVALID_ACTION;
	constexpr std::uint32_t kInvalidKey = DLMOD_HOTKEY_INVALID_KEY;

	struct TtlString {
		char* data;
		int length;
		int capacity;
	};

	struct RecordArray {
		void* items;
		int count;
		int capacity;
	};

	struct ConverterState {
		alignas(16) std::uint8_t bytes[kConverterStateSize];
	};

	using MallocFn = void*(__cdecl*)(std::size_t);
	using ReallocFn = void*(__cdecl*)(void*, std::size_t);
	using FreeFn = void(__cdecl*)(void*);
	using FindEntryFn = void*(__fastcall*)(void*, const TtlString*);
	using InsertNodeFn = void(__fastcall*)(void*, void*, char, void*);
	using CompileLayoutsFn = void*(__fastcall*)(void*, void*, std::uint8_t, std::uint8_t);
	using CopyStateFn = void*(__fastcall*)(void*, const void*);
	using ReloadInputsFn = void(__fastcall*)(void*);
	using LayoutApplyFn = void(__fastcall*)(void*, RecordArray*, int);

	struct GameHeap {
		MallocFn malloc = nullptr;
		ReallocFn realloc = nullptr;
		FreeFn free = nullptr;

		bool Ready() const noexcept
		{
			return malloc != nullptr && realloc != nullptr && free != nullptr;
		}
	};

	struct Hotkey {
		std::string id;
		std::string displayName;
		std::string pageTitle;
		std::string owner;
		std::uint32_t defaultKey = 0;
		std::uint32_t actionId = kInvalidAction;
		std::uint32_t boundKey = kInvalidKey;
		bool wasDown = false;
		void* userData = nullptr;
		DlModHotkeyCallbackV1 callback = nullptr;
	};

	struct PageGroup {
		std::string title;
		std::vector<std::size_t> hotkeyIndices;
	};

	GameHeap g_heap;
	HMODULE g_gameModule = nullptr;
	bool g_functionsResolved = false;
	bool g_unsupportedBuildLogged = false;

	FindEntryFn g_findEntry = nullptr;
	InsertNodeFn g_insertNode = nullptr;
	CompileLayoutsFn g_compileLayouts = nullptr;
	CopyStateFn g_copyState = nullptr;
	ReloadInputsFn g_originalReload = nullptr;

	std::recursive_mutex g_mutex;
	std::vector<Hotkey> g_hotkeys;
	thread_local const std::string* g_loadingMod = nullptr;
	bool g_initialized = false;
	bool g_hookInstalled = false;
	bool g_hookCreateFailed = false;
	bool g_injected = false;
	bool g_needsReload = false;
	std::uint64_t g_lastReloadMs = 0;
	std::uint64_t g_lastBindingRefreshMs = 0;
	std::uint64_t g_lastInjectionWarningMs = 0;
	std::uint32_t g_injectedGeneration = 0;

	GameHeap ResolveGameHeap() noexcept
	{
		GameHeap heap;
		HMODULE ucrt = GetModuleHandleW(L"ucrtbase.dll");
		if (ucrt == nullptr) {
			return heap;
		}
		heap.malloc = reinterpret_cast<MallocFn>(GetProcAddress(ucrt, "malloc"));
		heap.realloc = reinterpret_cast<ReallocFn>(GetProcAddress(ucrt, "realloc"));
		heap.free = reinterpret_cast<FreeFn>(GetProcAddress(ucrt, "free"));
		return heap;
	}

	GameHeap& Heap() noexcept
	{
		static GameHeap heap = ResolveGameHeap();
		return heap;
	}

	template <typename T>
	T ResolveRva(std::uint32_t rva) noexcept
	{
		if (g_gameModule == nullptr) {
			return nullptr;
		}
		return reinterpret_cast<T>(
			reinterpret_cast<std::uint8_t*>(g_gameModule) + rva);
	}

	void* AllocateGameMemory(std::size_t size) noexcept
	{
		GameHeap& heap = Heap();
		if (!heap.Ready()) {
			return nullptr;
		}
		void* memory = heap.malloc(size);
		if (memory != nullptr) {
			std::memset(memory, 0, size);
		}
		return memory;
	}

	void FreeGameMemory(void* memory) noexcept
	{
		GameHeap& heap = Heap();
		if (memory != nullptr && heap.free != nullptr) {
			heap.free(memory);
		}
	}

	bool AssignGameString(void* destination, const char* text) noexcept
	{
		if (destination == nullptr || text == nullptr || !Heap().Ready()) {
			return false;
		}
		const std::size_t length = std::strlen(text);
		char* copy = static_cast<char*>(Heap().malloc(length + 1));
		if (copy == nullptr) {
			return false;
		}
		std::memcpy(copy, text, length + 1);
		auto* value = static_cast<TtlString*>(destination);
		value->data = copy;
		value->length = static_cast<int>(length);
		value->capacity = static_cast<int>(length);
		return true;
	}

	bool ResolveGameFunctionsLocked() noexcept
	{
		if (g_functionsResolved) {
			return true;
		}
		if (g_gameModule == nullptr) {
			g_gameModule = GetModuleHandleW(kGameModuleName);
		}
		if (g_gameModule == nullptr) {
			return false;
		}
		if (!EngineEntityBindings::IsSupportedGame(g_gameModule)) {
			if (!g_unsupportedBuildLogged) {
				g_unsupportedBuildLogged = true;
				EngineLogger::Warning(
					"Hotkey runtime disabled: unsupported gamedll_x64_rwdi.dll build.");
			}
			g_functionsResolved = true;
			return false;
		}

		g_findEntry = ResolveRva<FindEntryFn>(kFindEntryByNameRva);
		g_insertNode = ResolveRva<InsertNodeFn>(kInsertTreeNodeRva);
		g_compileLayouts = ResolveRva<CompileLayoutsFn>(kCompileLayoutsRva);
		g_copyState = ResolveRva<CopyStateFn>(kCopyConverterStateRva);

		g_functionsResolved = g_findEntry != nullptr &&
			g_insertNode != nullptr &&
			g_compileLayouts != nullptr &&
			g_copyState != nullptr;
		if (!g_functionsResolved) {
			EngineLogger::Error("Hotkey runtime: failed to resolve gamedll functions.");
		}
		return g_functionsResolved;
	}

	bool AppendPointerVector(
		std::uint8_t* base,
		std::uint32_t pointerOffset,
		std::uint32_t countOffset,
		std::uint32_t capacityOffset,
		void* value) noexcept
	{
		auto** items = *reinterpret_cast<void***>(base + pointerOffset);
		int count = *reinterpret_cast<int*>(base + countOffset);
		int capacity = *reinterpret_cast<int*>(base + capacityOffset);

		if (count + 1 > capacity) {
			int newCapacity = (((10 * capacity) >> 3) + 16) & 0x3FFFFFF0;
			if (newCapacity < count + 1) {
				newCapacity = count + 1;
			}
			void* grown = Heap().realloc(items, sizeof(void*) * static_cast<std::size_t>(newCapacity));
			if (grown == nullptr) {
				return false;
			}
			items = static_cast<void**>(grown);
			*reinterpret_cast<void***>(base + pointerOffset) = items;
			*reinterpret_cast<int*>(base + capacityOffset) = newCapacity;
		}

		items[count] = value;
		*reinterpret_cast<int*>(base + countOffset) = count + 1;
		return true;
	}

	bool InsertActionId(void* treeHeader, int actionId) noexcept
	{
		auto* root = *reinterpret_cast<std::uint8_t**>(treeHeader);
		std::uint8_t* current = root;
		std::uint8_t* parent = nullptr;
		bool insertLeft = false;

		while (current != nullptr) {
			const int key = *reinterpret_cast<int*>(current - 8);
			if (key == actionId) {
				return true;
			}
			parent = current;
			if (actionId < key) {
				insertLeft = true;
				current = *reinterpret_cast<std::uint8_t**>(current);
			}
			else {
				insertLeft = false;
				current = *reinterpret_cast<std::uint8_t**>(current + 16);
			}
		}

		auto* allocation = static_cast<std::uint8_t*>(AllocateGameMemory(kTreeNodeSize));
		if (allocation == nullptr) {
			return false;
		}
		*reinterpret_cast<int*>(allocation) = actionId;
		g_insertNode(treeHeader, allocation + 8, insertLeft ? 1 : 0, parent);
		return true;
	}

	void* FindLayout(void* controllerContext, const char* name) noexcept
	{
		if (g_findEntry == nullptr || name == nullptr) {
			return nullptr;
		}
		TtlString lookup{
			const_cast<char*>(name),
			static_cast<int>(std::strlen(name)),
			static_cast<int>(std::strlen(name))};
		return g_findEntry(controllerContext, &lookup);
	}

	void* FindPreset(void* layout, const char* name) noexcept
	{
		if (layout == nullptr) {
			return nullptr;
		}
		auto** presets = *reinterpret_cast<void***>(
			static_cast<std::uint8_t*>(layout) + kLayoutPresetOffset);
		const int count = *reinterpret_cast<int*>(
			static_cast<std::uint8_t*>(layout) + kLayoutPresetCountOffset);
		if (presets == nullptr) {
			return nullptr;
		}
		const std::size_t length = name != nullptr ? std::strlen(name) : 0;
		for (int i = 0; i < count; ++i) {
			auto* preset = static_cast<std::uint8_t*>(presets[i]);
			if (preset == nullptr) {
				continue;
			}
			const auto* presetName = *reinterpret_cast<const char**>(preset);
			const int presetLength = *reinterpret_cast<int*>(preset + 8);
			if (presetLength == static_cast<int>(length) &&
				(length == 0 ||
					(presetName != nullptr &&
						std::memcmp(presetName, name, length) == 0))) {
				return preset;
			}
		}
		return nullptr;
	}

	std::string SanitizeIdentifier(const std::string& value)
	{
		std::string result;
		result.reserve(value.size());
		for (const char character : value) {
			const bool valid =
				(character >= 'a' && character <= 'z') ||
				(character >= 'A' && character <= 'Z') ||
				(character >= '0' && character <= '9') ||
				character == '_';
			result.push_back(valid ? character : '_');
		}
		if (result.empty()) {
			result = "hotkey";
		}
		return result;
	}

	bool AssignActionIdsLocked() noexcept
	{
		bool needsAssignment = false;
		for (const Hotkey& hotkey : g_hotkeys) {
			if (hotkey.actionId == kInvalidAction) {
				needsAssignment = true;
				break;
			}
		}
		if (!needsAssignment) {
			return true;
		}
		if (g_gameModule == nullptr) {
			return false;
		}

		const auto* descriptors = reinterpret_cast<const std::uint8_t*>(g_gameModule) +
			kDescriptorTableRva;
		std::array<bool, kMaxActionId + 1> used{};
		for (int i = 0; i < kDescriptorCount; ++i) {
			const int value = *reinterpret_cast<const int*>(
				descriptors + static_cast<std::size_t>(i) * 24 + 16);
			if (value >= 0 && value <= kMaxActionId) {
				used[static_cast<std::size_t>(value)] = true;
			}
		}

		/* 已知原版动作：FORWARD=4、BACKWARD=5、JUMP=37。
		   三者缺失说明描述符表尚未初始化或版本不匹配，稍后重试。 */
		if (!used[4] || !used[5] || !used[37]) {
			return false;
		}

		for (Hotkey& hotkey : g_hotkeys) {
			if (hotkey.actionId != kInvalidAction) {
				used[static_cast<std::size_t>(hotkey.actionId)] = true;
				continue;
			}
			for (int action = 1; action <= kMaxActionId; ++action) {
				if (!used[static_cast<std::size_t>(action)]) {
					used[static_cast<std::size_t>(action)] = true;
					hotkey.actionId = static_cast<std::uint32_t>(action);
					break;
				}
			}
			if (hotkey.actionId == kInvalidAction) {
				EngineLogger::Error("Hotkey runtime: no free input action slot available.");
				return false;
			}
		}
		return true;
	}

	void* CreateOperation(
		std::uint32_t actionId,
		std::uint32_t defaultKey) noexcept
	{
		auto* operation = static_cast<std::uint8_t*>(AllocateGameMemory(kOperationSize));
		if (operation == nullptr) {
			return nullptr;
		}
		*reinterpret_cast<void**>(operation) = ResolveRva<void*>(kAddActionVtableRva);
		*reinterpret_cast<int*>(operation + 0x08) = static_cast<int>(actionId);
		*reinterpret_cast<int*>(operation + 0x0C) = static_cast<int>(kDefaultTarget);
		*reinterpret_cast<int*>(operation + 0x10) =
			(defaultKey & 0xF00) == 0 ? static_cast<int>(kKeyboardDevice) : 0;
		*reinterpret_cast<int*>(operation + 0x14) = 0;
		*reinterpret_cast<int*>(operation + 0x18) = static_cast<int>(defaultKey);
		*reinterpret_cast<int*>(operation + 0x1C) = 0;
		*reinterpret_cast<int*>(operation + 0x20) = 0;
		*reinterpret_cast<int*>(operation + 0x24) = 0;
		return operation;
	}

	void* CreateKeybinding(
		std::uint32_t actionId,
		const std::string& internalName) noexcept
	{
		auto* keybinding = static_cast<std::uint8_t*>(AllocateGameMemory(kKeybindingSize));
		if (keybinding == nullptr) {
			return nullptr;
		}
		*reinterpret_cast<void**>(keybinding) = ResolveRva<void*>(kKeybindingVtableRva);
		if (!AssignGameString(keybinding + 0x08, internalName.c_str())) {
			FreeGameMemory(keybinding);
			return nullptr;
		}
		*reinterpret_cast<int*>(keybinding + 0x28) = 59;
		*reinterpret_cast<std::uint8_t*>(keybinding + 0x30) = 1;
		*reinterpret_cast<int*>(keybinding + 0x38) = kCategoryBase;
		*reinterpret_cast<int*>(keybinding + 0x3C) = kCollideBase;
		*reinterpret_cast<std::uint8_t*>(keybinding + 0x40) = 1;
		*reinterpret_cast<int*>(keybinding + 0x58) = 59;
		if (!InsertActionId(keybinding + kKeybindingTreeOffset, static_cast<int>(actionId))) {
			FreeGameMemory(keybinding);
			return nullptr;
		}
		return keybinding;
	}

	void* CreateView(const std::string& title) noexcept
	{
		auto* view = static_cast<std::uint8_t*>(AllocateGameMemory(kViewSize));
		if (view == nullptr) {
			return nullptr;
		}
		if (!AssignGameString(view, title.c_str())) {
			FreeGameMemory(view);
			return nullptr;
		}
		*reinterpret_cast<int*>(view + 0x20) = 59;
		*reinterpret_cast<int*>(view + 0x38) = 59;
		return view;
	}

	bool AppendViewRow(void* view, void* keybinding, const std::string& label) noexcept
	{
		auto* row = static_cast<std::uint8_t*>(AllocateGameMemory(kViewRowSize));
		if (row == nullptr) {
			return false;
		}
		*reinterpret_cast<void**>(row) = keybinding;
		if (!AssignGameString(row + 8, label.c_str())) {
			FreeGameMemory(row);
			return false;
		}
		auto* viewBytes = static_cast<std::uint8_t*>(view);
		return AppendPointerVector(
			viewBytes,
			kViewRowOffset,
			kViewRowCountOffset,
			kViewRowCapacityOffset,
			row);
	}

	bool RebuildConverterStateLocked(void* converter) noexcept
	{
		ConverterState state{};
		if (g_compileLayouts(converter, &state, 0, 0) == nullptr) {
			return false;
		}
		g_copyState(
			static_cast<std::uint8_t*>(converter) + kConverterStateOffset,
			&state);
		return true;
	}

	std::vector<PageGroup> GroupPagesLocked()
	{
		std::vector<PageGroup> pages;
		for (std::size_t i = 0; i < g_hotkeys.size(); ++i) {
			const std::string& title = g_hotkeys[i].pageTitle;
			auto iterator = std::find_if(
				pages.begin(),
				pages.end(),
				[&title](const PageGroup& page) { return page.title == title; });
			if (iterator == pages.end()) {
				pages.push_back(PageGroup{title, {i}});
			}
			else {
				iterator->hotkeyIndices.push_back(i);
			}
		}
		return pages;
	}

	void WarnInjectionThrottled(const char* message) noexcept
	{
		const std::uint64_t now = GetTickCount64();
		if (now - g_lastInjectionWarningMs < 5000) {
			return;
		}
		g_lastInjectionWarningMs = now;
		EngineLogger::Warning(message);
	}

	bool InjectAllLocked(void* converter) noexcept
	{
		if (converter == nullptr) {
			return false;
		}
		if (g_hotkeys.empty()) {
			g_injected = true;
			return true;
		}
		if (!ResolveGameFunctionsLocked()) {
			return false;
		}
		if (!AssignActionIdsLocked()) {
			return false;
		}

		constexpr int kControllers[] = {0, 2}; /* Keyboard, KeyboardAndPad */
		void* presets[2]{};
		void* controllerContexts[2]{};

		for (int index = 0; index < 2; ++index) {
			auto* context = static_cast<std::uint8_t*>(converter) +
				static_cast<std::uint32_t>(kControllers[index]) * kControllerStride;
			controllerContexts[index] = context;
			void* layout = FindLayout(context, "common_keyboard");
			if (layout == nullptr ||
				*reinterpret_cast<void**>(layout) != ResolveRva<void*>(kInputsLayoutVtableRva)) {
				WarnInjectionThrottled(
					"Hotkey runtime: common_keyboard layout not found; registration deferred.");
				return false;
			}
			void* preset = FindPreset(layout, "");
			if (preset == nullptr) {
				WarnInjectionThrottled(
					"Hotkey runtime: default preset not found; registration deferred.");
				return false;
			}
			presets[index] = preset;
		}

		const std::vector<PageGroup> pages = GroupPagesLocked();
		for (int index = 0; index < 2; ++index) {
			auto* context = static_cast<std::uint8_t*>(controllerContexts[index]);
			auto* preset = static_cast<std::uint8_t*>(presets[index]);

			for (const Hotkey& hotkey : g_hotkeys) {
				void* operation = CreateOperation(hotkey.actionId, hotkey.defaultKey);
				if (operation == nullptr ||
					!AppendPointerVector(
						preset,
						kPresetOperationOffset,
						kPresetOperationCountOffset,
						kPresetOperationCapacityOffset,
						operation)) {
					EngineLogger::Error("Hotkey runtime: failed to append action binding.");
					g_injected = true;
					return false;
				}
			}

			std::vector<std::pair<std::size_t, void*>> keybindings;
			keybindings.reserve(g_hotkeys.size());
			for (std::size_t i = 0; i < g_hotkeys.size(); ++i) {
				const Hotkey& hotkey = g_hotkeys[i];
				const std::string internalName =
					"Keybinding_Mod_" + SanitizeIdentifier(hotkey.id);
				void* keybinding = CreateKeybinding(hotkey.actionId, internalName);
				if (keybinding == nullptr ||
					!AppendPointerVector(
						context,
						kControllerEntryOffset,
						kControllerEntryCountOffset,
						kControllerEntryCapacityOffset,
						keybinding)) {
					EngineLogger::Error("Hotkey runtime: failed to append keybinding row.");
					g_injected = true;
					return false;
				}
				keybindings.emplace_back(i, keybinding);
			}

			for (const PageGroup& page : pages) {
				void* view = CreateView(page.title);
				if (view == nullptr) {
					EngineLogger::Error("Hotkey runtime: failed to create key binding page.");
					g_injected = true;
					return false;
				}
				for (const std::size_t hotkeyIndex : page.hotkeyIndices) {
					void* keybinding = nullptr;
					for (const auto& entry : keybindings) {
						if (entry.first == hotkeyIndex) {
							keybinding = entry.second;
							break;
						}
					}
					if (keybinding == nullptr ||
						!AppendViewRow(view, keybinding, g_hotkeys[hotkeyIndex].displayName)) {
						EngineLogger::Error("Hotkey runtime: failed to append key binding row.");
						g_injected = true;
						return false;
					}
				}
				if (!AppendPointerVector(
						context,
						kControllerViewOffset,
						kControllerViewCountOffset,
						kControllerViewCapacityOffset,
						view)) {
					EngineLogger::Error("Hotkey runtime: failed to append key binding page.");
					g_injected = true;
					return false;
				}
			}
		}

		if (!RebuildConverterStateLocked(converter)) {
			EngineLogger::Warning("Hotkey runtime: failed to rebuild converter state.");
		}

		++g_injectedGeneration;
		g_injected = true;
		g_needsReload = false;
		EngineLogger::Logf(
			EngineLogLevel::Info,
			"Hotkey runtime: injected %zu hotkey(s) across %zu page(s) (generation %u).",
			g_hotkeys.size(),
			pages.size(),
			g_injectedGeneration);
		return true;
	}

	bool IsProcessForeground() noexcept
	{
		const HWND foreground = GetForegroundWindow();
		if (foreground == nullptr) {
			return false;
		}
		DWORD processId = 0;
		GetWindowThreadProcessId(foreground, &processId);
		return processId == GetCurrentProcessId();
	}

	std::uint32_t DikToVirtualKey(std::uint32_t dik) noexcept
	{
		switch (dik) {
		case 0x01: return VK_ESCAPE;
		case 0x0E: return VK_BACK;
		case 0x0F: return VK_TAB;
		case 0x1C: return VK_RETURN;
		case 0x1D: return VK_LCONTROL;
		case 0x2A: return VK_LSHIFT;
		case 0x36: return VK_RSHIFT;
		case 0x37: return VK_MULTIPLY;
		case 0x38: return VK_LMENU;
		case 0x39: return VK_SPACE;
		case 0x3A: return VK_CAPITAL;
		case 0x45: return VK_NUMLOCK;
		case 0x46: return VK_SCROLL;
		case 0x9C: return VK_RETURN;
		case 0x9D: return VK_RCONTROL;
		case 0xB5: return VK_DIVIDE;
		case 0xB7: return VK_SNAPSHOT;
		case 0xB8: return VK_RMENU;
		case 0xC5: return VK_PAUSE;
		case 0xC7: return VK_HOME;
		case 0xC8: return VK_UP;
		case 0xC9: return VK_PRIOR;
		case 0xCB: return VK_LEFT;
		case 0xCD: return VK_RIGHT;
		case 0xCF: return VK_END;
		case 0xD0: return VK_DOWN;
		case 0xD1: return VK_NEXT;
		case 0xD2: return VK_INSERT;
		case 0xD3: return VK_DELETE;
		case 0xDB: return VK_LWIN;
		case 0xDC: return VK_RWIN;
		case 0xDD: return VK_APPS;
		default:
			break;
		}

		const UINT virtualKey = MapVirtualKeyW(dik, MAPVK_VSC_TO_VK_EX);
		return static_cast<std::uint32_t>(virtualKey);
	}

	bool IsKeyDown(std::uint32_t key) noexcept
	{
		if (key >= 0xF000 && key <= 0xF007) {
			const std::uint32_t button = key - 0xF000;
			const int virtualKey = button == 0 ? VK_LBUTTON
				: button == 1 ? VK_RBUTTON
				: button == 2 ? VK_MBUTTON
				: button == 3 ? VK_XBUTTON1
				: button == 4 ? VK_XBUTTON2
				: 0;
			return virtualKey != 0 &&
				(GetAsyncKeyState(virtualKey) & 0x8000) != 0;
		}
		if (key >= 0xF00) {
			/* 手柄/摇杆动作暂不轮询（v1 只支持键盘与鼠标侧键）。 */
			return false;
		}
		const std::uint32_t virtualKey = DikToVirtualKey(key);
		return virtualKey != 0 &&
			(GetAsyncKeyState(static_cast<int>(virtualKey)) & 0x8000) != 0;
	}

	bool BuildRecordArray(void* converter, RecordArray* records) noexcept
	{
		*records = {};
		if (converter == nullptr) {
			return false;
		}
		auto* context = static_cast<std::uint8_t*>(converter);
		auto** entries = *reinterpret_cast<void***>(
			context + kControllerEntryOffset);
		const int entryCount = *reinterpret_cast<int*>(
			context + kControllerEntryCountOffset);
		const auto* selectedPresets = *reinterpret_cast<const int**>(
			static_cast<std::uint8_t*>(converter) + kPerEntrySelectionBase);
		if (entries == nullptr || entryCount <= 0) {
			return false;
		}

		for (int i = 0; i < entryCount; ++i) {
			void* entry = entries[i];
			if (entry == nullptr) {
				continue;
			}
			auto** vtable = *reinterpret_cast<void***>(entry);
			if (vtable == nullptr || vtable[2] == nullptr) {
				continue;
			}
			const int presetIndex = selectedPresets != nullptr ? selectedPresets[i] : 0;
			const auto apply = reinterpret_cast<LayoutApplyFn>(vtable[2]);
			apply(entry, records, presetIndex);
		}
		return true;
	}

	void RefreshBindingsLocked(void* converter) noexcept
	{
		RecordArray records{};
		if (!BuildRecordArray(converter, &records)) {
			for (Hotkey& hotkey : g_hotkeys) {
				hotkey.boundKey = kInvalidKey;
				hotkey.wasDown = false;
			}
			return;
		}

		for (Hotkey& hotkey : g_hotkeys) {
			hotkey.boundKey = kInvalidKey;
			if (hotkey.actionId == kInvalidAction || records.items == nullptr) {
				continue;
			}
			const auto* bytes = static_cast<const std::uint8_t*>(records.items);
			for (int i = 0; i < records.count; ++i) {
				const auto* record = bytes + static_cast<std::size_t>(i) * 44;
				const int actionId = *reinterpret_cast<const int*>(record);
				if (actionId != static_cast<int>(hotkey.actionId)) {
					continue;
				}
				hotkey.boundKey = static_cast<std::uint32_t>(
					*reinterpret_cast<const int*>(record + 0x10));
				break;
			}
		}
		FreeGameMemory(records.items);
	}

	void PollLocked(void* converter) noexcept
	{
		if (!IsProcessForeground()) {
			for (Hotkey& hotkey : g_hotkeys) {
				hotkey.wasDown = false;
			}
			return;
		}

		const std::uint64_t now = GetTickCount64();
		if (now - g_lastBindingRefreshMs >= kBindingRefreshMilliseconds) {
			RefreshBindingsLocked(converter);
			g_lastBindingRefreshMs = now;
		}

		for (std::size_t i = 0; i < g_hotkeys.size(); ++i) {
			Hotkey& hotkey = g_hotkeys[i];
			const bool down = hotkey.boundKey != kInvalidKey && IsKeyDown(hotkey.boundKey);
			const bool shouldFire = down && !hotkey.wasDown;
			hotkey.wasDown = down;
			if (shouldFire && hotkey.callback != nullptr) {
				const auto callback = hotkey.callback;
				void* userData = hotkey.userData;
				const std::uint32_t actionId = hotkey.actionId;
				try {
					callback(userData, actionId);
				}
				catch (...) {
					EngineLogger::Warning("Hotkey runtime: a hotkey callback threw an exception.");
				}
			}
		}
	}

	void* GetConverterIfReadyLocked() noexcept
	{
		if (!ResolveGameFunctionsLocked() || g_gameModule == nullptr) {
			return nullptr;
		}
		auto* converter = reinterpret_cast<std::uint8_t*>(g_gameModule) + kConverterSingletonRva;
		/* 构造完成后 controller[0] 的名称字符串已分配。 */
		if (*reinterpret_cast<void**>(converter) == nullptr) {
			return nullptr;
		}
		if (*reinterpret_cast<int*>(converter + 8) <= 0) {
			return nullptr;
		}
		return converter;
	}

	void __fastcall ReloadInputsDetour(void* converter) noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		g_lastReloadMs = GetTickCount64();
		if (g_originalReload == nullptr) {
			return;
		}
		g_originalReload(converter);
		g_injected = false;
		if (!g_hotkeys.empty()) {
			InjectAllLocked(converter);
		}
		else {
			g_injected = true;
		}
	}

	bool TryInstallHookLocked() noexcept
	{
		if (g_hookInstalled) {
			return true;
		}
		if (g_hookCreateFailed) {
			return false;
		}
		if (g_gameModule == nullptr) {
			g_gameModule = GetModuleHandleW(kGameModuleName);
		}
		if (g_gameModule == nullptr || !EngineEntityBindings::IsSupportedGame(g_gameModule)) {
			return false;
		}

		const MH_STATUS initializeStatus = MH_Initialize();
		if (initializeStatus != MH_OK && initializeStatus != MH_ERROR_ALREADY_INITIALIZED) {
			EngineLogger::Error("Hotkey runtime: MinHook initialization failed.");
			g_hookCreateFailed = true;
			return false;
		}

		void* target = reinterpret_cast<void*>(
			reinterpret_cast<std::uint8_t*>(g_gameModule) + kReloadInputsRva);
		const MH_STATUS createStatus = MH_CreateHook(
			target,
			reinterpret_cast<void*>(&ReloadInputsDetour),
			reinterpret_cast<void**>(&g_originalReload));
		if (createStatus != MH_OK) {
			EngineLogger::Logf(
				EngineLogLevel::Warning,
				"Hotkey runtime: input reload hook creation failed (%d).",
				static_cast<int>(createStatus));
			g_hookCreateFailed = true;
			return false;
		}
		const MH_STATUS enableStatus = MH_EnableHook(target);
		if (enableStatus != MH_OK) {
			MH_RemoveHook(target);
			g_originalReload = nullptr;
			g_hookCreateFailed = true;
			EngineLogger::Error("Hotkey runtime: input reload hook could not be enabled.");
			return false;
		}
		g_hookInstalled = true;
		EngineLogger::Info("Hotkey runtime: input reload hook installed.");
		return true;
	}

}

void EngineHotkeyRuntime::Initialize() noexcept
{
	std::lock_guard<std::recursive_mutex> lock(g_mutex);
	g_initialized = true;
	TryInstallHookLocked();
}

void EngineHotkeyRuntime::Tick() noexcept
{
	std::lock_guard<std::recursive_mutex> lock(g_mutex);
	if (!g_initialized) {
		return;
	}
	TryInstallHookLocked();

	void* converter = GetConverterIfReadyLocked();
	if (converter == nullptr) {
		return;
	}

	if (!g_injected || g_needsReload) {
		if (g_needsReload && g_injected) {
			const std::uint64_t now = GetTickCount64();
			if (now - g_lastReloadMs >= kReloadDebounceMilliseconds) {
				EngineLogger::Info(
					"Hotkey runtime: reloading input bindings for late registration changes.");
				if (g_originalReload != nullptr) {
					g_originalReload(converter);
				}
				else {
					const auto reload = ResolveRva<ReloadInputsFn>(kReloadInputsRva);
					if (reload != nullptr) {
						reload(converter);
					}
				}
				g_lastReloadMs = now;
				g_injected = false;
				g_needsReload = false;
			}
		}
		if (!g_injected) {
			InjectAllLocked(converter);
		}
	}

	if (g_injected) {
		PollLocked(converter);
	}
}

void EngineHotkeyRuntime::Shutdown() noexcept
{
	std::lock_guard<std::recursive_mutex> lock(g_mutex);
	if (g_hookInstalled && g_gameModule != nullptr) {
		void* target = reinterpret_cast<void*>(
			reinterpret_cast<std::uint8_t*>(g_gameModule) + kReloadInputsRva);
		MH_DisableHook(target);
		MH_RemoveHook(target);
		g_hookInstalled = false;
	}
	g_originalReload = nullptr;
	g_injected = false;
	g_needsReload = false;
	g_initialized = false;
	g_hotkeys.clear();
}

const DlModHotkeyApiV1* EngineHotkeyRuntime::GetApi() noexcept
{
	static const DlModHotkeyApiV1 api{
		sizeof(DlModHotkeyApiV1),
		DLMOD_HOTKEY_ABI_VERSION,
		&EngineHotkeyRuntime::RegisterHotkey,
		&EngineHotkeyRuntime::GetHotkeyKey,
		&EngineHotkeyRuntime::GetHotkeyAction,
		&EngineHotkeyRuntime::UnregisterHotkey
	};
	return &api;
}

DlModResult MODSDK_CALL EngineHotkeyRuntime::RegisterHotkey(
	const DlModHotkeyDescV1* desc) noexcept
{
	if (desc == nullptr ||
		desc->structSize < offsetof(DlModHotkeyDescV1, callback) + sizeof(DlModHotkeyCallbackV1) ||
		desc->abiVersion != DLMOD_HOTKEY_ABI_VERSION ||
		desc->id == nullptr || desc->id[0] == '\0' ||
		desc->displayName == nullptr || desc->defaultKey == 0 ||
		desc->callback == nullptr) {
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}

	try {
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		for (const Hotkey& existing : g_hotkeys) {
			if (existing.id == desc->id) {
				return DLMOD_RESULT_INVALID_ARGUMENT;
			}
		}

		Hotkey hotkey;
		hotkey.id = desc->id;
		hotkey.displayName =
			desc->displayName[0] != '\0' ? desc->displayName : desc->id;
		hotkey.pageTitle =
			desc->pageTitle != nullptr && desc->pageTitle[0] != '\0'
				? desc->pageTitle
				: "Mods";
		hotkey.owner = g_loadingMod != nullptr ? *g_loadingMod : std::string();
		hotkey.defaultKey = desc->defaultKey;
		hotkey.userData = desc->userData;
		hotkey.callback = desc->callback;
		g_hotkeys.push_back(std::move(hotkey));
		if (g_injected) {
			g_needsReload = true;
		}
		return DLMOD_RESULT_SUCCESS;
	}
	catch (...) {
		return DLMOD_RESULT_INVALID_STATE;
	}
}

uint32_t MODSDK_CALL EngineHotkeyRuntime::GetHotkeyKey(const char* id) noexcept
{
	if (id == nullptr) {
		return kInvalidKey;
	}
	try {
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		for (const Hotkey& hotkey : g_hotkeys) {
			if (hotkey.id == id) {
				return hotkey.boundKey;
			}
		}
	}
	catch (...) {
	}
	return kInvalidKey;
}

uint32_t MODSDK_CALL EngineHotkeyRuntime::GetHotkeyAction(const char* id) noexcept
{
	if (id == nullptr) {
		return kInvalidAction;
	}
	try {
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		for (const Hotkey& hotkey : g_hotkeys) {
			if (hotkey.id == id) {
				return hotkey.actionId;
			}
		}
	}
	catch (...) {
	}
	return kInvalidAction;
}

DlModResult MODSDK_CALL EngineHotkeyRuntime::UnregisterHotkey(const char* id) noexcept
{
	if (id == nullptr) {
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	try {
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		const auto iterator = std::find_if(
			g_hotkeys.begin(),
			g_hotkeys.end(),
			[id](const Hotkey& hotkey) { return hotkey.id == id; });
		if (iterator == g_hotkeys.end()) {
			return DLMOD_RESULT_NOT_FOUND;
		}
		g_hotkeys.erase(iterator);
		if (g_injected) {
			g_needsReload = true;
		}
		return DLMOD_RESULT_SUCCESS;
	}
	catch (...) {
		return DLMOD_RESULT_INVALID_STATE;
	}
}

void EngineHotkeyRuntime::RemoveForOwner(const std::string& owner) noexcept
{
	try {
		std::lock_guard<std::recursive_mutex> lock(g_mutex);
		const auto previousSize = g_hotkeys.size();
		g_hotkeys.erase(
			std::remove_if(
				g_hotkeys.begin(),
				g_hotkeys.end(),
				[&owner](const Hotkey& hotkey) { return hotkey.owner == owner; }),
			g_hotkeys.end());
		if (g_hotkeys.size() != previousSize && g_injected) {
			g_needsReload = true;
		}
	}
	catch (...) {
	}
}

const std::string* EngineHotkeyRuntimeDetail::SetLoadingMod(const std::string* modId) noexcept
{
	const std::string* previous = g_loadingMod;
	g_loadingMod = modId;
	return previous;
}
