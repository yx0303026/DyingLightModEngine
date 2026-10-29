#include "runtime/EngineLobbyCollector.h"

#include "runtime/EngineHookRuntime.h"
#include "runtime/EngineLobbyData.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace
{
	constexpr char kHookOwner[] = "engine.lobby";
	constexpr std::int32_t kSuccess = 0;
	constexpr std::int32_t kSteamAccountType = 1;
	constexpr std::size_t kMaximumMembers = 64;

	struct CopyDetailsOptions final
	{
		std::int32_t api = 1;
		const char* lobbyId = nullptr;
		const void* localProductUserId = nullptr;
	};

	struct LobbyAttributeOptions final
	{
		std::int32_t api = 1;
		const char* key = nullptr;
	};

	struct MemberAttributeOptions final
	{
		std::int32_t api = 1;
		const void* member = nullptr;
		const char* key = nullptr;
	};

	struct MemberCountOptions final
	{
		std::int32_t api = 1;
	};

	struct MemberIndexOptions final
	{
		std::int32_t api = 1;
		std::uint32_t index = 0;
	};

	struct MemberAttributeCountOptions final
	{
		std::int32_t api = 1;
		const void* member = nullptr;
	};

	struct MemberAttributeIndexOptions final
	{
		std::int32_t api = 1;
		const void* member = nullptr;
		std::int32_t index = 0;
	};

	struct ExternalAccountOptions final
	{
		std::int32_t api = 1;
		std::int32_t accountType = kSteamAccountType;
		const void* productUserId = nullptr;
	};

	struct CopyInfoOptions final
	{
		std::int32_t api = 1;
		void* details = nullptr;
	};

	template<typename Value>
	bool ReadField(const void* object, std::uintptr_t offset, Value& value) noexcept
	{
		value = {};
		if (object == nullptr)
			return false;
		__try
		{
			value = *reinterpret_cast<const Value*>(
				static_cast<const std::uint8_t*>(object) + offset);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	bool CopyText(const char* value, char* output, std::size_t capacity) noexcept
	{
		if (output == nullptr || capacity == 0)
			return false;
		output[0] = '\0';
		if (value == nullptr)
			return false;
		__try
		{
			strncpy_s(output, capacity, value, capacity - 1);
			output[capacity - 1] = '\0';
			return output[0] != '\0';
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			output[0] = '\0';
			return false;
		}
	}

	bool ReadOptionPointer(const void* options, std::uintptr_t offset, void*& value) noexcept
	{
		return ReadField(options, offset, value);
	}

	bool ReadOptionString(const void* options, std::uintptr_t offset, char* output, std::size_t capacity) noexcept
	{
		const char* value = nullptr;
		return ReadField(options, offset, value) && CopyText(value, output, capacity);
	}

	bool ReadStringAttribute(
		void* details,
		const char* key,
		EngineLobbyCollector::Api* api,
		char* output,
		std::size_t capacity) noexcept
	{
		if (details == nullptr || key == nullptr || api == nullptr ||
			api->copyLobbyAttribute == nullptr)
			return false;
		LobbyAttributeOptions options{};
		options.key = key;
		void* attribute = nullptr;
		bool result = false;
		__try
		{
			if (api->copyLobbyAttribute(details, &options, &attribute) == kSuccess && attribute != nullptr)
			{
				void* data = nullptr;
				const char* value = nullptr;
				std::int32_t type = -1;
				if (ReadField(attribute, 0x08, data) && ReadField(data, 0x10, value) &&
					ReadField(data, 0x18, type) && type == 3)
				{
					result = CopyText(value, output, capacity);
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			result = false;
		}
		if (attribute != nullptr && api->releaseAttribute != nullptr)
			api->releaseAttribute(attribute);
		return result;
	}

	bool ReadMemberStringAttribute(
		void* details,
		const void* member,
		const char* key,
		EngineLobbyCollector::Api* api,
		char* output,
		std::size_t capacity) noexcept
	{
		if (details == nullptr || member == nullptr || key == nullptr || api == nullptr ||
			api->copyMemberAttribute == nullptr)
			return false;
		MemberAttributeOptions options{};
		options.member = member;
		options.key = key;
		void* attribute = nullptr;
		bool result = false;
		__try
		{
			if (api->copyMemberAttribute(details, &options, &attribute) == kSuccess && attribute != nullptr)
			{
				void* data = nullptr;
				const char* value = nullptr;
				std::int32_t type = -1;
				if (ReadField(attribute, 0x08, data) && ReadField(data, 0x10, value) &&
					ReadField(data, 0x18, type) && type == 3)
				{
					result = CopyText(value, output, capacity);
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			result = false;
		}
		if (attribute != nullptr && api->releaseAttribute != nullptr)
			api->releaseAttribute(attribute);
		return result;
	}

	void FillMemberAttributes(
		void* details,
		const void* member,
		EngineLobbyCollector::Api* api,
		EngineLobbyMemberRecord& result) noexcept
	{
		if (details == nullptr || member == nullptr || api == nullptr ||
			api->memberAttributeCount == nullptr || api->copyMemberAttributeByIndex == nullptr)
			return;
		MemberAttributeCountOptions countOptions{};
		countOptions.member = member;
		const std::int32_t count = api->memberAttributeCount(details, &countOptions);
		for (std::int32_t index = 0; index < count && index < 16; ++index)
		{
			MemberAttributeIndexOptions options{};
			options.member = member;
			options.index = index;
			void* attribute = nullptr;
			if (api->copyMemberAttributeByIndex(details, &options, &attribute) != kSuccess || attribute == nullptr)
				continue;
			void* data = nullptr;
			const char* value = nullptr;
			std::int32_t type = -1;
			if (ReadField(attribute, 0x08, data) && ReadField(data, 0x10, value) &&
				ReadField(data, 0x18, type) && type == 3)
			{
				char text[96]{};
				if (CopyText(value, text, sizeof(text)))
				{
					if (text[0] == ':' && result.steamId.empty())
						result.steamId = text + 1;
					else if (text[0] >= '0' && text[0] <= '9' && std::strlen(text) >= 15 && result.steamId.empty())
						result.steamId = text;
					else if (result.name.empty())
						result.name = text;
				}
			}
			if (api->releaseAttribute != nullptr)
				api->releaseAttribute(attribute);
		}

		if (api->memberAttributeCount == nullptr || api->copyMemberAttributeByIndex == nullptr)
			return;
		for (std::int32_t index = 0; index < count && index < 16; ++index)
		{
			MemberAttributeIndexOptions options{};
			options.member = member;
			options.index = index;
			void* attribute = nullptr;
			if (api->copyMemberAttributeByIndex(details, &options, &attribute) != kSuccess || attribute == nullptr)
				continue;
			void* data = nullptr;
			std::int32_t type = -1;
			void* value = nullptr;
			if (ReadField(attribute, 0x08, data) && ReadField(data, 0x18, type) &&
				ReadField(data, 0x10, value) && (type == 0 || type == 1))
			{
				const std::intptr_t integerValue = reinterpret_cast<std::intptr_t>(value);
				if (integerValue == 1)
				{
					result.team = "1";
					result.zombie = false;
				}
				else if (integerValue == 2)
				{
					result.team = "2";
					result.zombie = true;
				}
			}
			if (api->releaseAttribute != nullptr)
				api->releaseAttribute(attribute);
		}
	}

	bool CopyLobbyId(
		void* details,
		EngineLobbyCollector::Api* api,
		char* output,
		std::size_t capacity) noexcept
	{
		if (details == nullptr || api == nullptr || api->copyInfo == nullptr)
			return false;
		CopyInfoOptions options{};
		options.details = details;
		void* info = nullptr;
		bool result = false;
		if (api->copyInfo(details, &options, &info) == kSuccess && info != nullptr)
		{
			const char* lobbyId = nullptr;
			if (ReadField(info, 0x08, lobbyId))
				result = CopyText(lobbyId, output, capacity);
		}
		if (info != nullptr && api->releaseInfo != nullptr)
			api->releaseInfo(info);
		return result;
	}

	bool PuidToSteam(
		const void* puid,
		void* connectionInterface,
		EngineLobbyCollector::Api* api,
		char* output,
		std::size_t capacity) noexcept
	{
		if (puid == nullptr || connectionInterface == nullptr || api == nullptr ||
			api->copyExternalAccount == nullptr)
			return false;
		ExternalAccountOptions options{};
		options.productUserId = puid;
		void* info = nullptr;
		bool result = false;
		if (api->copyExternalAccount(connectionInterface, &options, &info) == kSuccess && info != nullptr)
		{
			const char* account = nullptr;
			if (ReadField(info, 0x18, account))
				result = CopyText(account, output, capacity);
		}
		if (info != nullptr && api->releaseExternalAccount != nullptr)
			api->releaseExternalAccount(info);
		return result;
	}

	bool IsValidPointer(const void* value) noexcept
	{
		return value != nullptr;
	}

	void* InvokeMemberGetter(
		void* slot,
		void* implementation,
		const void* options,
		void** output) noexcept
	{
		using GetMemberFunction = void*(*)(void*, const void*, void**);
		if (slot == nullptr || implementation == nullptr)
			return nullptr;
		__try
		{
			return reinterpret_cast<GetMemberFunction>(slot)(implementation, options, output);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
	}

	void MergeMember(
		std::vector<EngineLobbyMemberRecord>& members,
		const EngineLobbyMemberRecord& source) noexcept
	{
		for (EngineLobbyMemberRecord& destination : members)
		{
			if (destination.puid != source.puid)
				continue;
			if (!source.name.empty())
				destination.name = source.name;
			if (!source.steamId.empty())
				destination.steamId = source.steamId;
			if (!source.team.empty())
				destination.team = source.team;
			destination.zombie = destination.zombie || source.zombie;
			destination.hits += 1;
			return;
		}
		EngineLobbyMemberRecord copy = source;
		copy.hits = std::max<std::uint32_t>(copy.hits, 1);
		members.push_back(std::move(copy));
	}

}

EngineLobbyCollector& EngineLobbyCollector::Instance() noexcept
{
	static EngineLobbyCollector collector;
	return collector;
}

bool EngineLobbyCollector::Initialize() noexcept
{
	if (running_.exchange(true))
		return true;
	try
	{
		worker_ = std::thread(&EngineLobbyCollector::WorkerMain, this);
		return true;
	}
	catch (...)
	{
		running_.store(false);
		return false;
	}
}

void EngineLobbyCollector::Shutdown() noexcept
{
	running_.store(false);
	if (worker_.joinable())
		worker_.join();
	RemoveHooks();
	std::lock_guard<std::mutex> lock(stateMutex_);
	lobbyInterface_ = nullptr;
	connectionInterface_ = nullptr;
	platform_ = nullptr;
	localProductUserId_ = nullptr;
	details_ = nullptr;
	lobbyId_[0] = '\0';
	ownDetailsTried_ = false;
	api_ = {};
}

void EngineLobbyCollector::WorkerMain(EngineLobbyCollector* collector) noexcept
{
	if (collector == nullptr)
		return;
	while (collector->running_.load())
	{
		if (!collector->hooksInstalled_.load())
		{
			if (collector->ResolveInterfaces() && collector->InstallHooks())
				collector->hooksInstalled_.store(true);
		}
		if (collector->hooksInstalled_.load())
			collector->Refresh();
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}
}

bool EngineLobbyCollector::ResolveInterfaces() noexcept
{
	HMODULE eos = GetModuleHandleA("EOSSDK-Win64-Shipping.dll");
	if (eos == nullptr)
		return false;
	Api resolved{};
	resolved.copyDetails = reinterpret_cast<CopyDetailsFunction>(GetProcAddress(eos, "EOS_Lobby_CopyLobbyDetailsHandle"));
	resolved.memberCount = reinterpret_cast<MemberCountFunction>(GetProcAddress(eos, "EOS_LobbyDetails_GetMemberCount"));
	resolved.copyMemberAttribute = reinterpret_cast<CopyMemberAttributeFunction>(GetProcAddress(eos, "EOS_LobbyDetails_CopyMemberAttributeByKey"));
	resolved.copyLobbyAttribute = reinterpret_cast<CopyAttributeFunction>(GetProcAddress(eos, "EOS_LobbyDetails_CopyAttributeByKey"));
	resolved.memberAttributeCount = reinterpret_cast<MemberAttributeCountFunction>(GetProcAddress(eos, "EOS_LobbyDetails_GetMemberAttributeCount"));
	resolved.copyMemberAttributeByIndex = reinterpret_cast<CopyMemberAttributeByIndexFunction>(GetProcAddress(eos, "EOS_LobbyDetails_CopyMemberAttributeByIndex"));
	resolved.copyExternalAccount = reinterpret_cast<CopyExternalAccountFunction>(GetProcAddress(eos, "EOS_Connect_CopyProductUserExternalAccountByAccountType"));
	resolved.releaseExternalAccount = reinterpret_cast<ExternalAccountReleaseFunction>(GetProcAddress(eos, "EOS_Connect_ExternalAccountInfo_Release"));
	resolved.productUserIdToString = reinterpret_cast<ProductUserIdToStringFunction>(GetProcAddress(eos, "EOS_ProductUserId_ToString"));
	resolved.releaseAttribute = reinterpret_cast<AttributeReleaseFunction>(GetProcAddress(eos, "EOS_Lobby_Attribute_Release"));
	resolved.copyInfo = reinterpret_cast<CopyInfoFunction>(GetProcAddress(eos, "EOS_LobbyDetails_CopyInfo"));
	resolved.releaseInfo = reinterpret_cast<InfoReleaseFunction>(GetProcAddress(eos, "EOS_LobbyDetails_Info_Release"));
	resolved.getLobbyInterface = reinterpret_cast<GetLobbyInterfaceFunction>(GetProcAddress(eos, "EOS_Platform_GetLobbyInterface"));
	resolved.getConnectInterface = reinterpret_cast<GetConnectInterfaceFunction>(GetProcAddress(eos, "EOS_Platform_GetConnectInterface"));
	resolved.loggedInUser = reinterpret_cast<LoggedInUserFunction>(GetProcAddress(eos, "EOS_Connect_GetLoggedInUserByIndex"));
	resolved.copySearchResult = reinterpret_cast<CopySearchResultFunction>(GetProcAddress(eos, "EOS_LobbySearch_CopySearchResultByIndex"));
	if (resolved.copyDetails == nullptr || resolved.memberCount == nullptr ||
		resolved.copyMemberAttribute == nullptr || resolved.copyLobbyAttribute == nullptr ||
		resolved.memberAttributeCount == nullptr || resolved.copyMemberAttributeByIndex == nullptr ||
		resolved.productUserIdToString == nullptr || resolved.releaseAttribute == nullptr ||
		resolved.copyInfo == nullptr || resolved.releaseInfo == nullptr)
		return false;
	std::lock_guard<std::mutex> lock(stateMutex_);
	api_ = resolved;
	return true;
}

bool EngineLobbyCollector::InstallHooks() noexcept
{
	std::lock_guard<std::mutex> lock(stateMutex_);
	if (api_.copyDetails == nullptr)
		return false;
	const DlModResult copyResult = EngineHookRuntime::RegisterForMod(
		kHookOwner,
		reinterpret_cast<void*>(api_.copyDetails),
		reinterpret_cast<void*>(&HookCopyDetails),
		reinterpret_cast<void**>(&originalCopyDetails_));
	if (copyResult != DLMOD_RESULT_SUCCESS)
		return false;
	const DlModResult tickResult = EngineHookRuntime::RegisterForMod(
		kHookOwner,
		reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("EOSSDK-Win64-Shipping.dll"), "EOS_Platform_Tick")),
		reinterpret_cast<void*>(&HookPlatformTick),
		reinterpret_cast<void**>(&originalPlatformTick_));
	const DlModResult joinResult = EngineHookRuntime::RegisterForMod(
		kHookOwner,
		reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("EOSSDK-Win64-Shipping.dll"), "EOS_Lobby_JoinLobby")),
		reinterpret_cast<void*>(&HookJoinLobby),
		reinterpret_cast<void**>(&originalJoinLobby_));
	if (tickResult != DLMOD_RESULT_SUCCESS || joinResult != DLMOD_RESULT_SUCCESS)
	{
		EngineHookRuntime::RemoveForMod(kHookOwner);
		originalCopyDetails_ = nullptr;
		originalPlatformTick_ = nullptr;
		originalJoinLobby_ = nullptr;
		return false;
	}
	if (api_.copySearchResult != nullptr)
	{
		const DlModResult searchResult = EngineHookRuntime::RegisterForMod(
			kHookOwner,
			reinterpret_cast<void*>(api_.copySearchResult),
			reinterpret_cast<void*>(&HookCopySearchResult),
			reinterpret_cast<void**>(&originalCopySearchResult_));
		if (searchResult != DLMOD_RESULT_SUCCESS)
			originalCopySearchResult_ = nullptr;
	}
	return true;
}

void EngineLobbyCollector::RemoveHooks() noexcept
{
	if (!hooksInstalled_.exchange(false))
		return;
	EngineHookRuntime::RemoveForMod(kHookOwner);
	std::lock_guard<std::mutex> lock(stateMutex_);
	originalCopyDetails_ = nullptr;
	originalJoinLobby_ = nullptr;
	originalPlatformTick_ = nullptr;
	originalCopySearchResult_ = nullptr;
}

void EngineLobbyCollector::EnsureDetails() noexcept
{
	CopyDetailsFunction copyDetails = nullptr;
	void* lobbyInterface = nullptr;
	void* localProductUserId = nullptr;
	char lobbyId[64]{};
	{
		std::lock_guard<std::mutex> lock(stateMutex_);
		if (lobbyId_[0] == '\0' || ownDetailsTried_ || api_.copyDetails == nullptr ||
			lobbyInterface_ == nullptr || localProductUserId_ == nullptr)
			return;
		ownDetailsTried_ = true;
		copyDetails = api_.copyDetails;
		lobbyInterface = lobbyInterface_;
		localProductUserId = localProductUserId_;
		strncpy_s(lobbyId, sizeof(lobbyId), lobbyId_, _TRUNCATE);
	}
	CopyDetailsOptions options{};
	options.lobbyId = lobbyId;
	options.localProductUserId = localProductUserId;
	void* details = nullptr;
	if (copyDetails(lobbyInterface, &options, &details) == kSuccess && details != nullptr)
	{
		std::lock_guard<std::mutex> lock(stateMutex_);
		details_ = details;
	}
}

void EngineLobbyCollector::Refresh() noexcept
{
	EnsureDetails();
	Api api;
	void* details = nullptr;
	void* connection = nullptr;
	{
		std::lock_guard<std::mutex> lock(stateMutex_);
		api = api_;
		details = details_;
		connection = connectionInterface_;
	}
	if (details == nullptr || api.memberCount == nullptr || api.productUserIdToString == nullptr)
	{
		EngineLobbyData::Instance().SetStatus(DLMOD_LOBBY_STATUS_WAITING);
		return;
	}

	char lobbyId[64]{};
	CopyLobbyId(details, &api, lobbyId, sizeof(lobbyId));
	char ownerName[96]{};
	char ownerSteamId[48]{};
	char playersCountText[16]{};
	ReadStringAttribute(details, "NAME", &api, ownerName, sizeof(ownerName));
	ReadStringAttribute(details, "OWNER_ID", &api, ownerSteamId, sizeof(ownerSteamId));
	ReadStringAttribute(details, "PLAYERS_CNT", &api, playersCountText, sizeof(playersCountText));
	if (ownerSteamId[0] == ':')
		std::memmove(ownerSteamId, ownerSteamId + 1, std::strlen(ownerSteamId));
	const std::uint32_t reportedCount = playersCountText[0] == '\0'
		? 0U
		: static_cast<std::uint32_t>(std::max(0, std::atoi(playersCountText)));
	EngineLobbyData::Instance().SetLobbyMetadata(
		lobbyId[0] != '\0' ? lobbyId : nullptr,
		ownerName,
		ownerSteamId,
		reportedCount);

	MemberCountOptions countOptions{};
	const std::int32_t count = api.memberCount(details, &countOptions);
	if (count <= 0 || count > static_cast<std::int32_t>(kMaximumMembers))
	{
		EngineLobbyData::Instance().SetStatus(DLMOD_LOBBY_STATUS_STALE);
		return;
	}

	std::vector<EngineLobbyMemberRecord> members;
	members.reserve(static_cast<std::size_t>(count));
	void* implementation = nullptr;
	void* vtable = nullptr;
	void* slot = nullptr;
	if (!ReadField(details, 0, implementation) || !ReadField(implementation, 0, vtable) ||
		!ReadField(vtable, 0x40, slot) || slot == nullptr)
	{
		EngineLobbyData::Instance().SetStatus(DLMOD_LOBBY_STATUS_STALE);
		return;
	}
	for (std::int32_t index = 0; index < count; ++index)
	{
		MemberIndexOptions options{};
		options.index = static_cast<std::uint32_t>(index);
		void* output = nullptr;
		void* member = nullptr;
		member = InvokeMemberGetter(slot, implementation, &options, &output);
		if (!IsValidPointer(member) && IsValidPointer(output))
			member = output;
		if (member == nullptr)
			continue;
		char puid[64]{};
		std::int32_t length = static_cast<std::int32_t>(sizeof(puid));
		if (api.productUserIdToString(member, puid, &length) != kSuccess || std::strlen(puid) != 32)
			continue;
		EngineLobbyMemberRecord record{};
		record.puid = puid;
		char name[96]{};
		char steamId[48]{};
		char team[16]{};
		char zombie[16]{};
		ReadMemberStringAttribute(details, member, "NAME_s", &api, name, sizeof(name));
		ReadMemberStringAttribute(details, member, "PLAYER_ID_s", &api, steamId, sizeof(steamId));
		ReadMemberStringAttribute(details, member, "TEAM_l", &api, team, sizeof(team));
		ReadMemberStringAttribute(details, member, "ZOMBIE_b", &api, zombie, sizeof(zombie));
		record.name = name;
		record.steamId = steamId;
		record.team = team;
		record.zombie = std::strcmp(zombie, "true") == 0;
		FillMemberAttributes(details, member, &api, record);
		if (record.steamId.empty() && connection != nullptr)
		{
			char resolvedSteam[48]{};
			if (PuidToSteam(member, connection, &api, resolvedSteam, sizeof(resolvedSteam)))
				record.steamId = resolvedSteam;
		}
		MergeMember(members, record);
	}
	if (!members.empty())
	{
		EngineLobbyData::Instance().ReplaceCurrent(members);
		EngineLobbyData::Instance().MergeHistory(members);
	}
	else
		EngineLobbyData::Instance().SetStatus(DLMOD_LOBBY_STATUS_STALE);
}

std::int32_t EngineLobbyCollector::HookCopyDetails(
	void* lobbyInterface,
	const void* options,
	void** output) noexcept
{
	EngineLobbyCollector& collector = Instance();
	CopyDetailsFunction original = nullptr;
	{
		std::lock_guard<std::mutex> lock(collector.stateMutex_);
		original = collector.originalCopyDetails_;
	}
	const std::int32_t result = original == nullptr ? -1 : original(lobbyInterface, options, output);
	if (result != kSuccess || output == nullptr || *output == nullptr)
		return result;
	char lobbyId[64]{};
	ReadOptionString(options, 0x08, lobbyId, sizeof(lobbyId));
	Api api;
	{
		std::lock_guard<std::mutex> lock(collector.stateMutex_);
		api = collector.api_;
	}
	if (lobbyId[0] == '\0')
		CopyLobbyId(*output, &api, lobbyId, sizeof(lobbyId));
	{
		std::lock_guard<std::mutex> lock(collector.stateMutex_);
		collector.lobbyInterface_ = lobbyInterface;
		collector.details_ = *output;
		if (lobbyId[0] != '\0' && std::strcmp(lobbyId, collector.lobbyId_) != 0)
		{
			strncpy_s(collector.lobbyId_, sizeof(collector.lobbyId_), lobbyId, _TRUNCATE);
			collector.ownDetailsTried_ = false;
			EngineLobbyData::Instance().ResetLobby(collector.lobbyId_);
		}
	}
	return result;
}

std::int32_t EngineLobbyCollector::HookJoinLobby(
	void* lobbyInterface,
	const void* options,
	void* callback,
	void* clientData) noexcept
{
	EngineLobbyCollector& collector = Instance();
	char lobbyId[64]{};
	ReadOptionString(options, 0x08, lobbyId, sizeof(lobbyId));
	void* localPuid = nullptr;
	ReadOptionPointer(options, 0x10, localPuid);
	{
		std::lock_guard<std::mutex> lock(collector.stateMutex_);
		collector.lobbyInterface_ = lobbyInterface;
		if (localPuid != nullptr)
			collector.localProductUserId_ = localPuid;
		if (lobbyId[0] != '\0' && std::strcmp(lobbyId, collector.lobbyId_) != 0)
		{
			strncpy_s(collector.lobbyId_, sizeof(collector.lobbyId_), lobbyId, _TRUNCATE);
			collector.details_ = nullptr;
			collector.ownDetailsTried_ = false;
			EngineLobbyData::Instance().ResetLobby(collector.lobbyId_);
		}
	}
	JoinLobbyFunction original = nullptr;
	{
		std::lock_guard<std::mutex> lock(collector.stateMutex_);
		original = collector.originalJoinLobby_;
	}
	return original == nullptr ? -1 : original(lobbyInterface, options, callback, clientData);
}

std::int32_t EngineLobbyCollector::HookPlatformTick(
	void* platform,
	const void* options) noexcept
{
	EngineLobbyCollector& collector = Instance();
	{
		std::lock_guard<std::mutex> lock(collector.stateMutex_);
		collector.platform_ = platform;
		if (collector.api_.getLobbyInterface != nullptr)
			collector.lobbyInterface_ = collector.api_.getLobbyInterface(platform);
		if (collector.api_.getConnectInterface != nullptr)
			collector.connectionInterface_ = collector.api_.getConnectInterface(platform);
		if (collector.connectionInterface_ != nullptr && collector.api_.loggedInUser != nullptr &&
			collector.localProductUserId_ == nullptr)
			collector.localProductUserId_ = const_cast<void*>(collector.api_.loggedInUser(collector.connectionInterface_, 0));
	}
	PlatformTickFunction original = nullptr;
	{
		std::lock_guard<std::mutex> lock(collector.stateMutex_);
		original = collector.originalPlatformTick_;
	}
	return original == nullptr ? -1 : original(platform, options);
}

std::int32_t EngineLobbyCollector::HookCopySearchResult(
	void* search,
	const void* options,
	void** output) noexcept
{
	EngineLobbyCollector& collector = Instance();
	CopySearchResultFunction original = nullptr;
	Api api;
	{
		std::lock_guard<std::mutex> lock(collector.stateMutex_);
		original = collector.originalCopySearchResult_;
		api = collector.api_;
	}
	const std::int32_t result = original == nullptr ? -1 : original(search, options, output);
	if (result != kSuccess || output == nullptr || *output == nullptr)
		return result;
	EngineLobbyRoomRecord room{};
	char lobbyId[64]{};
	if (CopyLobbyId(*output, &api, lobbyId, sizeof(lobbyId)))
	{
		room.lobbyId = lobbyId;
		char ownerName[96]{};
		char ownerSteamId[48]{};
		ReadStringAttribute(*output, "NAME", &api, ownerName, sizeof(ownerName));
		ReadStringAttribute(*output, "OWNER_ID", &api, ownerSteamId, sizeof(ownerSteamId));
		room.ownerName = ownerName;
		room.ownerSteamId = ownerSteamId;
		if (!room.ownerSteamId.empty() && room.ownerSteamId.front() == ':')
			room.ownerSteamId.erase(room.ownerSteamId.begin());
		EngineLobbyData::Instance().AddRoom(room);
	}
	return result;
}

