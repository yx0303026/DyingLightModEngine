#include "managers/EngineModManager.h"

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace {

	using ModPath = std::filesystem::path;

	struct LoadedMod {
		HMODULE module = nullptr;
		std::string id;
		ModPath path;
	};

	struct EngineModManagerState {
		std::recursive_mutex mutex;
		std::vector<LoadedMod> loadedMods;
	};

	EngineModManagerState g_engineModManagerState;

	EngineModManagerState& GetState() noexcept
	{
		return g_engineModManagerState;
	}

	bool ComparePaths(const ModPath& left, const ModPath& right)
	{
		return left.filename().wstring() < right.filename().wstring();
	}

	bool IsDirectorySeparator(wchar_t value) noexcept
	{
		return value == L'\\' || value == L'/';
	}

	bool IsValidModName(const std::wstring& modName) noexcept
	{
		if (modName.empty() || modName == L"." || modName == L"..") {
			return false;
		}

		for (const wchar_t value : modName) {
			if (IsDirectorySeparator(value) || value == L':') {
				return false;
			}
		}
		return true;
	}

	std::wstring ConvertUtf8ToWide(const char* value)
	{
		if (value == nullptr || value[0] == '\0') {
			return {};
		}

		const int requiredLength = MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			value,
			-1,
			nullptr,
			0);
		if (requiredLength <= 0) {
			return {};
		}

		std::wstring result(static_cast<size_t>(requiredLength), L'\0');
		if (MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			value,
			-1,
			result.data(),
			requiredLength) <= 0) {
			return {};
		}
		result.resize(static_cast<size_t>(requiredLength - 1));
		return result;
	}

	ModPath GetExecutableDirectory()
	{
		std::vector<wchar_t> buffer(512);
		for (;;) {
			const DWORD length = GetModuleFileNameW(
				nullptr,
				buffer.data(),
				static_cast<DWORD>(buffer.size()));
			if (length == 0) {
				return {};
			}
			if (length < buffer.size() - 1) {
				ModPath path(buffer.data(), buffer.data() + length);
				return path.parent_path();
			}
			buffer.resize(buffer.size() * 2);
			if (buffer.size() > 32768) {
				return {};
			}
		}
	}

	ModPath ResolveModsDirectory(const char* modsDirectory)
	{
		if (modsDirectory == nullptr || modsDirectory[0] == '\0') {
			const ModPath executableDirectory = GetExecutableDirectory();
			return executableDirectory.empty() ? ModPath{} : executableDirectory / L"Mods";
		}

		const std::wstring wideDirectory = ConvertUtf8ToWide(modsDirectory);
		if (wideDirectory.empty()) {
			return {};
		}

		std::error_code error;
		const ModPath directory = std::filesystem::absolute(ModPath(wideDirectory), error);
		return error ? ModPath{} : directory.lexically_normal();
	}

	std::wstring ToSnakeCase(const std::wstring& value)
	{
		std::wstring result;
		result.reserve(value.size() + 4);

		bool previousWasSeparator = false;
		for (size_t index = 0; index < value.size(); ++index) {
			const wchar_t character = value[index];
			if (character == L'-' || character == L' ' || character == L'.') {
				if (!result.empty() && !previousWasSeparator) {
					result.push_back(L'_');
				}
				previousWasSeparator = true;
				continue;
			}

			if (character >= L'A' && character <= L'Z') {
				if (!result.empty() && !previousWasSeparator && index > 0) {
					result.push_back(L'_');
				}
				result.push_back(static_cast<wchar_t>(character - L'A' + L'a'));
				previousWasSeparator = false;
				continue;
			}

			result.push_back(character);
			previousWasSeparator = false;
		}

		while (!result.empty() && result.back() == L'_') {
			result.pop_back();
		}
		return result;
	}

	bool IsRegularFile(const ModPath& path) noexcept
	{
		std::error_code error;
		return std::filesystem::is_regular_file(path, error) && !error;
	}

	bool HasDllExtension(const ModPath& path) noexcept
	{
		const std::wstring extension = path.extension().wstring();
		return extension.size() == 4 &&
			(extension[1] == L'd' || extension[1] == L'D') &&
			(extension[2] == L'l' || extension[2] == L'L') &&
			(extension[3] == L'l' || extension[3] == L'L');
	}

	bool IsDynamicLibrary(const ModPath& path) noexcept
	{
		return HasDllExtension(path) && IsRegularFile(path);
	}

	ModPath FindModLibrary(const ModPath& modsDirectory, const std::wstring& modName)
	{
		const ModPath modDirectory = modsDirectory / modName;
		std::error_code error;
		if (std::filesystem::is_directory(modDirectory, error) && !error) {
			const std::wstring snakeCaseName = ToSnakeCase(modName);
			if (!snakeCaseName.empty()) {
				const ModPath snakeCaseLibrary = modDirectory / (snakeCaseName + L".dll");
				if (IsRegularFile(snakeCaseLibrary)) {
					return snakeCaseLibrary;
				}
			}

			const ModPath nameLibrary = modDirectory / (modName + L".dll");
			if (IsRegularFile(nameLibrary)) {
				return nameLibrary;
			}
		}

		if (HasDllExtension(modName)) {
			const ModPath explicitLibrary = modsDirectory / modName;
			return IsRegularFile(explicitLibrary) ? explicitLibrary : ModPath{};
		}

		const ModPath flatLibrary = modsDirectory / (modName + L".dll");
		return IsRegularFile(flatLibrary) ? flatLibrary : ModPath{};
	}

	std::vector<ModPath> FindModDirectories(const ModPath& modsDirectory)
	{
		std::vector<ModPath> directories;
		std::error_code error;
		std::filesystem::directory_iterator iterator(modsDirectory, error);
		if (error) {
			return directories;
		}

		for (const std::filesystem::directory_entry& entry : iterator) {
			std::error_code entryError;
			if (entry.is_directory(entryError) && !entryError) {
				directories.push_back(entry.path());
			}
		}

		std::sort(directories.begin(), directories.end(), &ComparePaths);
		return directories;
	}

	std::vector<ModPath> FindModLibraries(const ModPath& modsDirectory)
	{
		std::vector<ModPath> libraries;
		std::error_code error;
		std::filesystem::directory_iterator iterator(modsDirectory, error);
		if (error) {
			return libraries;
		}

		for (const std::filesystem::directory_entry& entry : iterator) {
			std::error_code entryError;
			if (entry.is_regular_file(entryError) && !entryError && IsDynamicLibrary(entry.path())) {
				libraries.push_back(entry.path());
			}
		}

		std::sort(libraries.begin(), libraries.end(), &ComparePaths);
		return libraries;
	}

	std::vector<LoadedMod>::iterator FindLoadedMod(
		std::vector<LoadedMod>& loadedMods,
		const std::string& modId)
	{
		for (auto iterator = loadedMods.begin(); iterator != loadedMods.end(); ++iterator) {
			if (iterator->id == modId) {
				return iterator;
			}
		}
		return loadedMods.end();
	}

	bool QueryEngineApi(ModEngineApiV1& api) noexcept
	{
		api = {};
		api.structSize = sizeof(api);
		if (ModSDK_QueryApi(MODSDK_ABI_VERSION, &api) != DLMOD_RESULT_SUCCESS) {
			return false;
		}
		return api.structSize >= sizeof(ModEngineApiV1) &&
			api.abiVersion == MODSDK_ABI_VERSION &&
			api.registerMod != nullptr &&
			api.unregisterMod != nullptr &&
			api.renderFrame != nullptr &&
			api.isUiOpen != nullptr &&
			api.onWindowMessage != nullptr;
	}

	DlModResult LoadModLibrary(
		const ModPath& libraryPath,
		std::vector<LoadedMod>& loadedMods)
	{
		const HMODULE module = LoadLibraryW(libraryPath.c_str());
		if (module == nullptr) {
			return DLMOD_RESULT_IO_ERROR;
		}

		const auto queryMod = reinterpret_cast<DlModQueryModFn>(
			GetProcAddress(module, "ModSDK_QueryMod"));
		if (queryMod == nullptr) {
			FreeLibrary(module);
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}

		DlModDescriptorV1 descriptor{};
		descriptor.structSize = sizeof(descriptor);
		DlModResult queryResult = DLMOD_RESULT_INVALID_STATE;
		try {
			queryResult = queryMod(MODSDK_ABI_VERSION, &descriptor);
		}
		catch (...) {
			queryResult = DLMOD_RESULT_INVALID_STATE;
		}

		if (queryResult != DLMOD_RESULT_SUCCESS ||
			descriptor.structSize < DLMOD_DESCRIPTOR_V1_MIN_SIZE ||
			descriptor.abiVersion != MODSDK_ABI_VERSION ||
			descriptor.id == nullptr ||
			descriptor.id[0] == '\0' ||
			descriptor.name == nullptr ||
			descriptor.version == nullptr ||
			descriptor.onLoad == nullptr) {
			FreeLibrary(module);
			return queryResult == DLMOD_RESULT_SUCCESS ? DLMOD_RESULT_INVALID_ARGUMENT : queryResult;
		}

		const std::string modId(descriptor.id);
		if (FindLoadedMod(loadedMods, modId) != loadedMods.end()) {
			FreeLibrary(module);
			return DLMOD_RESULT_INVALID_STATE;
		}

		ModEngineApiV1 engineApi{};
		if (!QueryEngineApi(engineApi)) {
			FreeLibrary(module);
			return DLMOD_RESULT_INVALID_STATE;
		}

		const DlModResult registerResult = engineApi.registerMod(&descriptor);
		if (registerResult != DLMOD_RESULT_SUCCESS) {
			FreeLibrary(module);
			return registerResult;
		}

		try {
			loadedMods.push_back({ module, modId, libraryPath });
		}
		catch (...) {
			engineApi.unregisterMod(modId.c_str());
			FreeLibrary(module);
			return DLMOD_RESULT_INVALID_STATE;
		}
		return DLMOD_RESULT_SUCCESS;
	}

	DlModResult LoadModByName(
		const std::wstring& modName,
		const ModPath& modsDirectory,
		std::vector<LoadedMod>& loadedMods)
	{
		if (!IsValidModName(modName)) {
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}

		const ModPath libraryPath = FindModLibrary(modsDirectory, modName);
		if (libraryPath.empty()) {
			return DLMOD_RESULT_NOT_FOUND;
		}
		return LoadModLibrary(libraryPath, loadedMods);
	}

	DlModResult UnloadLoadedMod(
		const std::string& modId,
		std::vector<LoadedMod>& loadedMods)
	{
		const auto iterator = FindLoadedMod(loadedMods, modId);
		if (iterator == loadedMods.end()) {
			return DLMOD_RESULT_NOT_FOUND;
		}

		ModEngineApiV1 engineApi{};
		if (!QueryEngineApi(engineApi)) {
			return DLMOD_RESULT_INVALID_STATE;
		}

		const DlModResult unregisterResult = engineApi.unregisterMod(modId.c_str());
		if (unregisterResult != DLMOD_RESULT_SUCCESS) {
			return unregisterResult;
		}

		const HMODULE module = iterator->module;
		loadedMods.erase(iterator);
		return FreeLibrary(module) ? DLMOD_RESULT_SUCCESS : DLMOD_RESULT_IO_ERROR;
	}

}

DlModResult EngineModManager::LoadAll(const char* modsDirectory) noexcept
{
	try {
		const ModPath root = ResolveModsDirectory(modsDirectory);
		if (root.empty()) {
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}
		if (!std::filesystem::exists(root)) {
			return DLMOD_RESULT_SUCCESS;
		}
		if (!std::filesystem::is_directory(root)) {
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}

		const std::lock_guard<std::recursive_mutex> lock(GetState().mutex);
		const std::vector<ModPath> directories = FindModDirectories(root);
		const std::vector<ModPath> libraries = FindModLibraries(root);
		bool loadedAny = false;
		bool failedAny = false;
		for (const ModPath& directory : directories) {
			const DlModResult result = LoadModByName(directory.filename().wstring(), root, GetState().loadedMods);
			if (result == DLMOD_RESULT_SUCCESS) {
				loadedAny = true;
			}
			else if (result != DLMOD_RESULT_NOT_FOUND) {
				failedAny = true;
			}
		}
		for (const ModPath& library : libraries) {
			const DlModResult result = LoadModLibrary(library, GetState().loadedMods);
			if (result == DLMOD_RESULT_SUCCESS) {
				loadedAny = true;
			}
			else {
				failedAny = true;
			}
		}

		if (failedAny && loadedAny) {
			return DLMOD_RESULT_PARTIAL_SUCCESS;
		}
		if (failedAny) {
			return DLMOD_RESULT_IO_ERROR;
		}
		return DLMOD_RESULT_SUCCESS;
	}
	catch (...) {
		return DLMOD_RESULT_INVALID_STATE;
	}
}

DlModResult EngineModManager::Load(const char* modName, const char* modsDirectory) noexcept
{
	try {
		const std::wstring wideName = ConvertUtf8ToWide(modName);
		const ModPath root = ResolveModsDirectory(modsDirectory);
		if (!IsValidModName(wideName) || root.empty()) {
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}

		const std::lock_guard<std::recursive_mutex> lock(GetState().mutex);
		return LoadModByName(wideName, root, GetState().loadedMods);
	}
	catch (...) {
		return DLMOD_RESULT_INVALID_STATE;
	}
}

DlModResult EngineModManager::Unload(const char* modId) noexcept
{
	try {
		const std::string id = modId == nullptr ? std::string{} : std::string(modId);
		if (id.empty()) {
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}

		const std::lock_guard<std::recursive_mutex> lock(GetState().mutex);
		return UnloadLoadedMod(id, GetState().loadedMods);
	}
	catch (...) {
		return DLMOD_RESULT_INVALID_STATE;
	}
}

DlModResult EngineModManager::UnloadAll() noexcept
{
	try {
		const std::lock_guard<std::recursive_mutex> lock(GetState().mutex);
		std::vector<std::string> ids;
		ids.reserve(GetState().loadedMods.size());
		for (const LoadedMod& mod : GetState().loadedMods) {
			ids.push_back(mod.id);
		}

		bool failedAny = false;
		for (auto iterator = ids.rbegin(); iterator != ids.rend(); ++iterator) {
			if (UnloadLoadedMod(*iterator, GetState().loadedMods) != DLMOD_RESULT_SUCCESS) {
				failedAny = true;
			}
		}
		return failedAny ? DLMOD_RESULT_PARTIAL_SUCCESS : DLMOD_RESULT_SUCCESS;
	}
	catch (...) {
		return DLMOD_RESULT_INVALID_STATE;
	}
}

uint32_t EngineModManager::GetLoadedCount() noexcept
{
	try {
		const std::lock_guard<std::recursive_mutex> lock(GetState().mutex);
		return static_cast<uint32_t>(GetState().loadedMods.size());
	}
	catch (...) {
		return 0;
	}
}

void EngineModManager::RenderFrame(void* graphicsContext) noexcept
{
	try {
		ModEngineApiV1 engineApi{};
		if (!QueryEngineApi(engineApi)) {
			return;
		}
		engineApi.renderFrame(graphicsContext);
	}
	catch (...) {
	}
}

bool EngineModManager::IsUiOpen() noexcept
{
	try {
		ModEngineApiV1 engineApi{};
		if (!QueryEngineApi(engineApi)) {
			return false;
		}
		return engineApi.isUiOpen() != 0U;
	}
	catch (...) {
		return false;
	}
}

void EngineModManager::OnWindowMessage(
	uintptr_t window,
	uint32_t message,
	uintptr_t wParam,
	intptr_t lParam) noexcept
{
	try {
		ModEngineApiV1 engineApi{};
		if (QueryEngineApi(engineApi)) {
			engineApi.onWindowMessage(window, message, wParam, lParam);
		}
	}
	catch (...) {
	}
}
