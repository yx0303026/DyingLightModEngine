#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

// 负责在 Engine 内部接入 EOS 大厅数据并刷新 EngineLobbyData。
class EngineLobbyCollector final
{
public:
	static EngineLobbyCollector& Instance() noexcept;

	bool Initialize() noexcept;
	void Shutdown() noexcept;

public:
	using LobbyDetailsHandle = void*;
	using CopyInfoFunction = std::int32_t(*)(void*, const void*, void**);
	using InfoReleaseFunction = std::int32_t(*)(void*);
	using CopyAttributeFunction = std::int32_t(*)(void*, const void*, void**);
	using MemberCountFunction = std::int32_t(*)(void*, const void*);
	using CopyMemberAttributeFunction = std::int32_t(*)(void*, const void*, void**);
	using MemberAttributeCountFunction = std::int32_t(*)(void*, const void*);
	using CopyMemberAttributeByIndexFunction = std::int32_t(*)(void*, const void*, void**);
	using AttributeReleaseFunction = std::int32_t(*)(void*);
	using CopyExternalAccountFunction = std::int32_t(*)(void*, const void*, void**);
	using ExternalAccountReleaseFunction = std::int32_t(*)(void*);
	using ProductUserIdToStringFunction = std::int32_t(*)(const void*, char*, std::int32_t*);
	using GetLobbyInterfaceFunction = void*(*)(void*);
	using GetConnectInterfaceFunction = void*(*)(void*);
	using LoggedInUserFunction = const void*(*)(void*, std::int32_t);
	using GetMemberByIndexFunction = const void*(*)(void*, std::int32_t);
	using CopyDetailsFunction = std::int32_t(*)(void*, const void*, void**);
	using DetailsReleaseFunction = void(*)(void*);
	using CopySearchResultFunction = std::int32_t(*)(void*, const void*, void**);

	struct Api final
	{
		CopyDetailsFunction copyDetails = nullptr;
		MemberCountFunction memberCount = nullptr;
		CopyMemberAttributeFunction copyMemberAttribute = nullptr;
		CopyAttributeFunction copyLobbyAttribute = nullptr;
		MemberAttributeCountFunction memberAttributeCount = nullptr;
		CopyMemberAttributeByIndexFunction copyMemberAttributeByIndex = nullptr;
		CopyExternalAccountFunction copyExternalAccount = nullptr;
		ExternalAccountReleaseFunction releaseExternalAccount = nullptr;
		ProductUserIdToStringFunction productUserIdToString = nullptr;
		AttributeReleaseFunction releaseAttribute = nullptr;
		CopyInfoFunction copyInfo = nullptr;
		InfoReleaseFunction releaseInfo = nullptr;
		GetLobbyInterfaceFunction getLobbyInterface = nullptr;
		GetConnectInterfaceFunction getConnectInterface = nullptr;
		LoggedInUserFunction loggedInUser = nullptr;
		GetMemberByIndexFunction getMemberByIndex = nullptr;
		CopySearchResultFunction copySearchResult = nullptr;
		DetailsReleaseFunction releaseDetails = nullptr;
	};

private:

	EngineLobbyCollector() = default;
	~EngineLobbyCollector() = default;
	EngineLobbyCollector(const EngineLobbyCollector&) = delete;
	EngineLobbyCollector& operator=(const EngineLobbyCollector&) = delete;

	static void WorkerMain(EngineLobbyCollector* collector) noexcept;
	void Refresh() noexcept;
	bool ResolveInterfaces() noexcept;
	bool InstallHooks() noexcept;
	void RemoveHooks() noexcept;
	void EnsureDetails() noexcept;
	void UpdateLobbyTarget(
		void* lobbyInterface,
		const char* lobbyId,
		const void* localProductUserId,
		bool onlyIfUnset) noexcept;
	void SetStatusIfCurrent(std::uint64_t generation, std::uint32_t status) noexcept;

	static std::int32_t HookCopyDetails(void* lobbyInterface, const void* options, void** output) noexcept;
	static std::int32_t HookJoinLobby(
		void* lobbyInterface,
		const void* options,
		void* callback,
		void* clientData) noexcept;
	static std::int32_t HookPlatformTick(void* platform, const void* options) noexcept;
	static std::int32_t HookCopySearchResult(void* search, const void* options, void** output) noexcept;

	std::atomic_bool running_ = false;
	std::atomic_bool hooksInstalled_ = false;
	std::thread worker_;
	void* lobbyInterface_ = nullptr;
	void* connectionInterface_ = nullptr;
	void* platform_ = nullptr;
	void* localProductUserId_ = nullptr;
	LobbyDetailsHandle ownedDetails_ = nullptr;
	char lobbyId_[64]{};
	char ownedDetailsLobbyId_[64]{};
	std::uint64_t lobbyGeneration_ = 0;
	std::uint64_t ownedDetailsGeneration_ = 0;
	bool ownedDetailsInvalidated_ = false;
	std::chrono::steady_clock::time_point nextDetailsRetry_{};

	using JoinLobbyFunction = std::int32_t(*)(void*, const void*, void*, void*);
	using PlatformTickFunction = std::int32_t(*)(void*, const void*);
	CopyDetailsFunction originalCopyDetails_ = nullptr;
	JoinLobbyFunction originalJoinLobby_ = nullptr;
	PlatformTickFunction originalPlatformTick_ = nullptr;
	CopySearchResultFunction originalCopySearchResult_ = nullptr;
	Api api_{};
	std::mutex stateMutex_;
};

