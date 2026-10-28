#include "PakLoader.h"

#include "PakLoaderAbi.h"

#include <algorithm>
#include <atomic>
#include <cwchar>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace custom_pak_mod {
	namespace {

		struct LoaderState {
			void* hostContext = nullptr;
			EngineLogCallback hostLog = nullptr;
			std::atomic<bool> stopRequested = false;
			std::thread worker;
		};

		struct ProviderContext {
			LoaderState* state = nullptr;
			HMODULE fileSystemModule = nullptr;
			DlFileSystemAddSourceFn addSource = nullptr;
			std::vector<std::string> sources;
		};

		LoaderState g_loaderState;

		std::string ToUtf8(const std::wstring& value)
		{
			if (value.empty()) {
				return {};
			}
			const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
				value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
			if (required <= 0) {
				return {};
			}
			std::string result(static_cast<size_t>(required), '\0');
			if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.c_str(),
				static_cast<int>(value.size()), result.data(), required, nullptr, nullptr) != required) {
				return {};
			}
			return result;
		}

		void Log(LoaderState& state, uint32_t level, const char* message)
		{
			if (state.hostLog != nullptr) {
				state.hostLog(state.hostContext, level, message);
			}
		}

		std::filesystem::path GetModuleDirectory(HMODULE module)
		{
			std::wstring path(32768, L'\0');
			const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
			if (length == 0 || length >= path.size()) {
				return {};
			}
			path.resize(length);
			return std::filesystem::path(path).parent_path();
		}

		bool ScanPakDirectory(const std::filesystem::path& directory, ProviderContext& context)
		{
			std::error_code error;
			if (!std::filesystem::is_directory(directory, error) || error) {
				return false;
			}

			for (std::filesystem::directory_iterator iterator(directory, error), end;
				!error && iterator != end; iterator.increment(error)) {
				if (!iterator->is_regular_file(error) || error) {
					if (error) break;
					continue;
				}
				if (_wcsicmp(iterator->path().extension().c_str(), L".pak") != 0) {
					continue;
				}
				const std::string source = ToUtf8(iterator->path().native());
				if (!source.empty()) {
					context.sources.push_back(source);
				}
			}

			std::sort(context.sources.begin(), context.sources.end());
			return !error;
		}

		DlModResult MODSDK_CALL ResolveFileSystem(
			void* opaqueContext,
			uint32_t timeoutMilliseconds,
			DlFileSystemRuntimeV1* runtime)
		{
			if (opaqueContext == nullptr || runtime == nullptr ||
				runtime->structSize < sizeof(DlFileSystemRuntimeV1) ||
				runtime->abiVersion != DLMOD_FILE_SYSTEM_ABI_VERSION) {
				return DLMOD_RESULT_INVALID_ARGUMENT;
			}
			auto& context = *static_cast<ProviderContext*>(opaqueContext);
			const ULONGLONG deadline = GetTickCount64() + timeoutMilliseconds;
			do {
				if (context.state->stopRequested.load()) {
					return DLMOD_RESULT_INVALID_STATE;
				}
				HMODULE module = GetModuleHandleA(DLMOD_RWDI_FILESYSTEM_MODULE_NAME_UTF8);
				if (module != nullptr) {
					auto addSource = reinterpret_cast<DlFileSystemAddSourceFn>(
						GetProcAddress(module, DLMOD_RWDI_FILESYSTEM_ADD_SOURCE_SYMBOL));
					if (addSource == nullptr) {
						return DLMOD_RESULT_NOT_FOUND;
					}
					context.fileSystemModule = module;
					context.addSource = addSource;
					runtime->moduleHandle = module;
					runtime->addSource = addSource;
					runtime->pakMountFlags = DLMOD_RWDI_FILESYSTEM_DEFAULT_PAK_FLAGS;
					return DLMOD_RESULT_SUCCESS;
				}
				Sleep(100);
			} while (GetTickCount64() < deadline);
			return DLMOD_RESULT_NOT_FOUND;
		}

		DlModResult MODSDK_CALL MountSource(
			void* opaqueContext,
			const char* sourceUtf8,
			int32_t mountFlags,
			uint32_t* lastError)
		{
			if (opaqueContext == nullptr || sourceUtf8 == nullptr || lastError == nullptr) {
				return DLMOD_RESULT_INVALID_ARGUMENT;
			}
			auto& context = *static_cast<ProviderContext*>(opaqueContext);
			if (context.addSource == nullptr) {
				return DLMOD_RESULT_INVALID_STATE;
			}
			SetLastError(ERROR_SUCCESS);
			const bool mounted = context.addSource(sourceUtf8, mountFlags);
			*lastError = GetLastError();
			return mounted ? DLMOD_RESULT_SUCCESS : DLMOD_RESULT_IO_ERROR;
		}

		DlModResult MODSDK_CALL GetAdditionalSource(void* opaqueContext, uint32_t index, const char** sourceUtf8)
		{
			if (opaqueContext == nullptr || sourceUtf8 == nullptr) {
				return DLMOD_RESULT_INVALID_ARGUMENT;
			}
			auto& sources = static_cast<ProviderContext*>(opaqueContext)->sources;
			if (index >= sources.size()) {
				return DLMOD_RESULT_NOT_FOUND;
			}
			*sourceUtf8 = sources[index].c_str();
			return DLMOD_RESULT_SUCCESS;
		}

		void MODSDK_CALL ProviderLog(void* opaqueContext, uint32_t level, const char* messageUtf8)
		{
			if (opaqueContext != nullptr && messageUtf8 != nullptr) {
				Log(*static_cast<ProviderContext*>(opaqueContext)->state, level, messageUtf8);
			}
		}

		DlModResult MODSDK_CALL LoadConfigured(
			const DlPakLoaderOptionsV1* options,
			const DlFileSystemPakProviderV1* fileSystem,
			DlPakLoaderReportV1* report)
		{
			if (options == nullptr || fileSystem == nullptr || report == nullptr ||
				options->structSize < sizeof(DlPakLoaderOptionsV1) ||
				options->abiVersion != DLMOD_FILE_SYSTEM_ABI_VERSION ||
				fileSystem->structSize < sizeof(DlFileSystemPakProviderV1) ||
				fileSystem->abiVersion != DLMOD_FILE_SYSTEM_ABI_VERSION ||
				fileSystem->resolveFileSystem == nullptr || fileSystem->mountSource == nullptr ||
				fileSystem->getAdditionalSource == nullptr) {
				return DLMOD_RESULT_INVALID_ARGUMENT;
			}

			DlFileSystemRuntimeV1 runtime{ sizeof(DlFileSystemRuntimeV1), DLMOD_FILE_SYSTEM_ABI_VERSION };
			const DlModResult resolveResult = fileSystem->resolveFileSystem(
				fileSystem->context, options->fileSystemTimeoutMilliseconds, &runtime);
			if (resolveResult != DLMOD_RESULT_SUCCESS) {
				return resolveResult;
			}
			const int32_t flags = options->mountFlags != 0 ? options->mountFlags : runtime.pakMountFlags;
			for (uint32_t index = 0;; ++index) {
				const char* source = nullptr;
				const DlModResult sourceResult = fileSystem->getAdditionalSource(fileSystem->context, index, &source);
				if (sourceResult == DLMOD_RESULT_NOT_FOUND) {
					break;
				}
				if (sourceResult != DLMOD_RESULT_SUCCESS || source == nullptr) {
					++report->sourcesFailed;
					continue;
				}
				++report->sourcesFound;
				uint32_t lastError = ERROR_SUCCESS;
				const DlModResult mountResult = fileSystem->mountSource(
					fileSystem->context, source, flags, &lastError);
				if (mountResult == DLMOD_RESULT_SUCCESS) {
					++report->sourcesMounted;
				}
				else {
					++report->sourcesFailed;
					report->lastError = lastError;
				}
			}
			return report->sourcesFailed == 0 ? DLMOD_RESULT_SUCCESS : DLMOD_RESULT_PARTIAL_SUCCESS;
		}

		void RunLoader(HMODULE ownerModule)
		{
			LoaderState& state = g_loaderState;
			try {
				ProviderContext context{ &state };
				const std::filesystem::path moduleDirectory = GetModuleDirectory(ownerModule);
				if (moduleDirectory.empty() || !ScanPakDirectory(moduleDirectory / L"CustomPak", context)) {
					Log(state, 3, "CustomPakMod: CustomPak directory is missing or could not be scanned.");
					return;
				}
				if (context.sources.empty()) {
					Log(state, 2, "CustomPakMod: no .pak files found in CustomPak.");
					return;
				}

				DlFileSystemPakProviderV1 provider{
					sizeof(DlFileSystemPakProviderV1), DLMOD_FILE_SYSTEM_ABI_VERSION,
					&context, &ResolveFileSystem, &MountSource, &GetAdditionalSource, &ProviderLog
				};
				const std::string modulePath = ToUtf8(moduleDirectory.native());
				const std::string pakDirectory = ToUtf8((moduleDirectory / L"CustomPak").native());
				DlPakLoaderOptionsV1 options{};
				options.structSize = sizeof(DlPakLoaderOptionsV1);
				options.abiVersion = DLMOD_FILE_SYSTEM_ABI_VERSION;
				options.flags = DLMOD_PAK_LOADER_ADDITIONAL_SOURCES_FIRST;
				options.mountFlags = DLMOD_RWDI_FILESYSTEM_DEFAULT_PAK_FLAGS;
				options.moduleDirectoryUtf8 = modulePath.c_str();
				options.defaultPakDirectoryUtf8 = pakDirectory.c_str();
				options.fileSystemTimeoutMilliseconds = 30000;

				DlPakLoaderReportV1 report{ sizeof(DlPakLoaderReportV1) };
				const DlModResult result = LoadConfigured(&options, &provider, &report);
				Log(state, result == DLMOD_RESULT_SUCCESS ? 2u : 3u,
					result == DLMOD_RESULT_SUCCESS
					? "CustomPakMod: all pak files mounted."
					: "CustomPakMod: pak loading completed with failures or could not resolve filesystem.");
			}
			catch (...) {
				Log(state, 4, "CustomPakMod: unexpected error while loading pak files.");
			}
		}

	}

	bool StartPakLoading(HMODULE ownerModule, const DlModHostApiV1* hostApi) noexcept
	{
		if (ownerModule == nullptr || hostApi == nullptr || g_loaderState.worker.joinable()) {
			return false;
		}
		g_loaderState.hostContext = hostApi->context;
		g_loaderState.hostLog = hostApi->log;
		g_loaderState.stopRequested.store(false);
		try {
			g_loaderState.worker = std::thread(RunLoader, ownerModule);
		}
		catch (...) {
			g_loaderState.hostContext = nullptr;
			g_loaderState.hostLog = nullptr;
			return false;
		}
		return true;
	}

	void StopPakLoading() noexcept
	{
		g_loaderState.stopRequested.store(true);
		if (g_loaderState.worker.joinable()) {
			g_loaderState.worker.join();
		}
		g_loaderState.hostContext = nullptr;
		g_loaderState.hostLog = nullptr;
	}

}
