#pragma once

#include "sdk/EngineHostAbi.h"

#include <Windows.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace player_name_mask
{
	// 区域概览：负责本地玩家名称遮罩、阵营编号分配和游戏函数挂接。
	class PlayerNameMaskRuntime final
	{
	public:
		PlayerNameMaskRuntime() = default;
		~PlayerNameMaskRuntime() = default;
		PlayerNameMaskRuntime(const PlayerNameMaskRuntime&) = delete;
		PlayerNameMaskRuntime& operator=(const PlayerNameMaskRuntime&) = delete;

		DlModResult Initialize(const DlModHostApiV1* hostApi) noexcept;
		void Shutdown() noexcept;

	private:
		// 区域概览：保存游戏字符串布局和本地编号映射。
		struct EngineWideString final
		{
			const wchar_t* data = nullptr;
			std::uint64_t length = 0;
		};

		enum class Team : std::uint8_t
		{
			Human,
			Hunter
		};

		struct NameRecord final
		{
			Team team = Team::Human;
			std::uint32_t index = 0;
		};

		using GetPlayerNameFunction = EngineWideString* (__fastcall*)(void* object);

		// 区域概览：静态 Hook 入口和本地名称生成。
		static EngineWideString* __fastcall GetPlayerNameDetour(void* object) noexcept;
		EngineWideString* HandlePlayerName(void* object) noexcept;
		EngineWideString* BuildMaskedName(Team team, std::uint32_t index) noexcept;
		NameRecord GetOrCreateRecord(void* object, Team team) noexcept;

		// 区域概览：模块、PE 镜像和 RTTI 安全检查。
		static HMODULE FindGameModule() noexcept;
		static bool ReadImageSize(HMODULE module, std::uint32_t& imageSize) noexcept;
		static bool ResolveTarget(HMODULE module, void** target) noexcept;
		static bool ReadTypeName(
			const void* object,
			char* buffer,
			std::size_t capacity) noexcept;
		static Team ClassifyTeam(const void* object) noexcept;
		static bool IsPlayerObject(const char* typeName) noexcept;
		static bool IsHunterType(const char* typeName) noexcept;

		// 区域概览：运行状态字段集中声明，便于卸载和版本失效时复位。
		static PlayerNameMaskRuntime* activeRuntime_;
		static GetPlayerNameFunction originalGetPlayerName_;

		const DlModHostApiV1* hostApi_ = nullptr;
		void* hostContext_ = nullptr;
		EngineLogCallback hostLog_ = nullptr;
		std::mutex recordsMutex_;
		std::unordered_map<void*, NameRecord> records_;
		std::uint32_t nextHumanIndex_ = 1;
		std::uint32_t nextHunterIndex_ = 1;
		bool initialized_ = false;
	};
}
