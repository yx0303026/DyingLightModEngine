#include "PlayerNameMaskRuntime.h"

#include "logging/EngineLogger.h"

#include <array>
#include <cstring>
#include <cwchar>
#include <string_view>

namespace player_name_mask
{
	namespace
	{
		// 区域概览：游戏版本绑定和安全上限。
		constexpr wchar_t kGameModuleName[] = L"gamedll_x64_rwdi.dll";
		constexpr std::uintptr_t kGetPlayerNameRva = 0x491E00;
		constexpr std::uint32_t kMinimumImageSize =
			static_cast<std::uint32_t>(kGetPlayerNameRva + 0x100);
		constexpr std::array<std::uint8_t, 16> kGetPlayerNameSignature = {
			0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
			0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x65
		};
		constexpr std::size_t kTypeNameCapacity = 256;
		constexpr std::size_t kMaskedNameCapacity = 64;

		struct RttiCompleteObjectLocator final
		{
			std::uint32_t signature = 0;
			std::uint32_t offset = 0;
			std::uint32_t constructorDisplacement = 0;
			std::int32_t typeDescriptorRva = 0;
			std::int32_t classDescriptorRva = 0;
			std::int32_t selfRva = 0;
		};

		bool ContainsInsensitive(const char* value, const char* term) noexcept
		{
			if (value == nullptr || term == nullptr)
				return false;
			const std::string_view text(value);
			const std::string_view needle(term);
			if (needle.empty() || text.size() < needle.size())
				return false;
			for (std::size_t offset = 0; offset + needle.size() <= text.size(); ++offset)
			{
				bool matched = true;
				for (std::size_t index = 0; index < needle.size(); ++index)
				{
					const char left = text[offset + index];
					const char right = needle[index];
					const char normalizedLeft =
						left >= 'A' && left <= 'Z' ? static_cast<char>(left + ('a' - 'A')) : left;
					const char normalizedRight =
						right >= 'A' && right <= 'Z' ? static_cast<char>(right + ('a' - 'A')) : right;
					if (normalizedLeft != normalizedRight)
					{
						matched = false;
						break;
					}
				}
				if (matched)
					return true;
			}
			return false;
		}
	}

	PlayerNameMaskRuntime* PlayerNameMaskRuntime::activeRuntime_ = nullptr;
	PlayerNameMaskRuntime::GetPlayerNameFunction
		PlayerNameMaskRuntime::originalGetPlayerName_ = nullptr;

	DlModResult PlayerNameMaskRuntime::Initialize(const DlModHostApiV1* hostApi) noexcept
	{
		if (initialized_ || hostApi == nullptr ||
			hostApi->structSize < DLMOD_HOST_API_V1_HOOKS_SIZE ||
			hostApi->abiVersion != MODSDK_ABI_VERSION ||
			hostApi->log == nullptr || hostApi->hooks == nullptr ||
			hostApi->hooks->structSize < DLMOD_HOOK_API_V1_MIN_SIZE ||
			hostApi->hooks->abiVersion != MODSDK_ABI_VERSION ||
			hostApi->hooks->registerHook == nullptr)
		{
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}

		HMODULE gameModule = FindGameModule();
		void* target = nullptr;
		if (!ResolveTarget(gameModule, &target))
			return DLMOD_RESULT_UNSUPPORTED_FEATURE;

		const DlModResult hookResult = hostApi->hooks->registerHook(
			target,
			reinterpret_cast<void*>(&GetPlayerNameDetour),
			reinterpret_cast<void**>(&originalGetPlayerName_));
		if (hookResult != DLMOD_RESULT_SUCCESS)
		{
			originalGetPlayerName_ = nullptr;
			return hookResult;
		}

		hostApi_ = hostApi;
		hostContext_ = hostApi->context;
		hostLog_ = hostApi->log;
		activeRuntime_ = this;
		initialized_ = true;
		hostLog_(
			hostContext_,
			static_cast<std::uint32_t>(EngineLogLevel::Info),
			"Player Name Mask loaded; local player names are masked only in the client display path.");
		return DLMOD_RESULT_SUCCESS;
	}

	void PlayerNameMaskRuntime::Shutdown() noexcept
	{
		if (!initialized_)
			return;

		{
			const std::lock_guard<std::mutex> lock(recordsMutex_);
			records_.clear();
			nextHumanIndex_ = 1;
			nextHunterIndex_ = 1;
		}
		if (activeRuntime_ == this)
			activeRuntime_ = nullptr;
		originalGetPlayerName_ = nullptr;
		if (hostLog_ != nullptr)
		{
			hostLog_(
				hostContext_,
				static_cast<std::uint32_t>(EngineLogLevel::Info),
				"Player Name Mask unloaded.");
		}
		hostApi_ = nullptr;
		hostContext_ = nullptr;
		hostLog_ = nullptr;
		initialized_ = false;
	}

	PlayerNameMaskRuntime::EngineWideString*
		__fastcall PlayerNameMaskRuntime::GetPlayerNameDetour(void* object) noexcept
	{
		PlayerNameMaskRuntime* runtime = activeRuntime_;
		if (runtime == nullptr)
			return originalGetPlayerName_ == nullptr ? nullptr : originalGetPlayerName_(object);
		return runtime->HandlePlayerName(object);
	}

	PlayerNameMaskRuntime::EngineWideString*
		PlayerNameMaskRuntime::HandlePlayerName(void* object) noexcept
	{
		if (originalGetPlayerName_ == nullptr)
			return nullptr;

		EngineWideString* original = nullptr;
		__try
		{
			original = originalGetPlayerName_(object);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
		if (original == nullptr || object == nullptr)
			return original;

		char typeName[kTypeNameCapacity]{};
		if (!ReadTypeName(object, typeName, sizeof(typeName)) || !IsPlayerObject(typeName))
			return original;

		const Team team = ClassifyTeam(object);
		const NameRecord record = GetOrCreateRecord(object, team);
		if (record.index == 0)
			return original;
		return BuildMaskedName(record.team, record.index);
	}

	PlayerNameMaskRuntime::EngineWideString*
		PlayerNameMaskRuntime::BuildMaskedName(Team team, std::uint32_t index) noexcept
	{
		thread_local wchar_t maskedName[kMaskedNameCapacity]{};
		thread_local EngineWideString result{};
		const wchar_t* prefix = team == Team::Hunter ? L"猎手" : L"队友";
		_snwprintf_s(
			maskedName,
			_countof(maskedName),
			_TRUNCATE,
			L"%s%u",
			prefix,
			index);
		result.data = maskedName;
		result.length = std::wcslen(maskedName);
		return &result;
	}

	PlayerNameMaskRuntime::NameRecord
		PlayerNameMaskRuntime::GetOrCreateRecord(void* object, Team team) noexcept
	{
		try
		{
			const std::lock_guard<std::mutex> lock(recordsMutex_);
			const auto existing = records_.find(object);
			if (existing != records_.end() && existing->second.team == team)
				return existing->second;

			NameRecord record{};
			record.team = team;
			if (team == Team::Hunter)
				record.index = nextHunterIndex_++;
			else
				record.index = nextHumanIndex_++;
			records_[object] = record;
			return record;
		}
		catch (...)
		{
			return {};
		}
	}

	HMODULE PlayerNameMaskRuntime::FindGameModule() noexcept
	{
		return GetModuleHandleW(kGameModuleName);
	}

	bool PlayerNameMaskRuntime::ReadImageSize(
		HMODULE module,
		std::uint32_t& imageSize) noexcept
	{
		imageSize = 0;
		if (module == nullptr)
			return false;
		__try
		{
			const auto* base = reinterpret_cast<const std::uint8_t*>(module);
			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
				dos->e_lfanew > (1 << 20))
				return false;
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
			if (nt->Signature != IMAGE_NT_SIGNATURE ||
				nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
				nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
				return false;
			imageSize = nt->OptionalHeader.SizeOfImage;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	bool PlayerNameMaskRuntime::ResolveTarget(HMODULE module, void** target) noexcept
	{
		if (target == nullptr)
			return false;
		*target = nullptr;
		std::uint32_t imageSize = 0;
		if (!ReadImageSize(module, imageSize) || imageSize < kMinimumImageSize)
			return false;
		auto* address = reinterpret_cast<std::uint8_t*>(module) + kGetPlayerNameRva;
		__try
		{
			if (std::memcmp(address, kGetPlayerNameSignature.data(), kGetPlayerNameSignature.size()) != 0)
				return false;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
		*target = address;
		return *target != nullptr;
	}

	bool PlayerNameMaskRuntime::ReadTypeName(
		const void* object,
		char* buffer,
		std::size_t capacity) noexcept
	{
		if (buffer == nullptr || capacity == 0)
			return false;
		buffer[0] = '\0';
		if (object == nullptr)
			return false;
		__try
		{
			const auto* vtable = *reinterpret_cast<void* const* const*>(object);
			if (vtable == nullptr || vtable[-1] == nullptr)
				return false;
			const auto* locator =
				static_cast<const RttiCompleteObjectLocator*>(vtable[-1]);
			if (locator->signature != 1 || locator->selfRva <= 0 ||
				locator->typeDescriptorRva <= 0)
				return false;
			const auto imageBase = reinterpret_cast<std::uintptr_t>(locator) -
				static_cast<std::uint32_t>(locator->selfRva);
			std::uint32_t imageSize = 0;
			if (!ReadImageSize(reinterpret_cast<HMODULE>(imageBase), imageSize))
				return false;
			const auto typeRva = static_cast<std::uint32_t>(locator->typeDescriptorRva);
			if (typeRva >= imageSize || imageSize - typeRva < 17)
				return false;
			const char* name = reinterpret_cast<const char*>(imageBase + typeRva + 16);
			for (std::size_t index = 0; index + 1 < capacity; ++index)
			{
				buffer[index] = name[index];
				if (buffer[index] == '\0')
					return true;
			}
			buffer[capacity - 1] = '\0';
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			buffer[0] = '\0';
			return false;
		}
	}

	PlayerNameMaskRuntime::Team
		PlayerNameMaskRuntime::ClassifyTeam(const void* object) noexcept
	{
		char typeName[kTypeNameCapacity]{};
		if (!ReadTypeName(object, typeName, sizeof(typeName)))
			return Team::Human;
		return IsHunterType(typeName) ? Team::Hunter : Team::Human;
	}

	bool PlayerNameMaskRuntime::IsPlayerObject(const char* typeName) noexcept
	{
		return ContainsInsensitive(typeName, "player") ||
			ContainsInsensitive(typeName, "hunter") ||
			ContainsInsensitive(typeName, "btz");
	}

	bool PlayerNameMaskRuntime::IsHunterType(const char* typeName) noexcept
	{
		return ContainsInsensitive(typeName, "hunter") ||
			ContainsInsensitive(typeName, "playerzombie") ||
			ContainsInsensitive(typeName, "btz");
	}
}
