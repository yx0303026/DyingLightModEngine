#include "runtime/EngineProjectileManager.h"

#include "logging/EngineLogger.h"
#include "runtime/EngineEntityBindings.h"

#include <Windows.h>
#include <MinHook.h>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace
{
	using BulletUpdateFunction = void(__fastcall*)(void*, float, std::uint8_t);
	using FrameUpdateFunction = void(__fastcall*)(void*);

	/* 锁定构建 gamedll_x64_rwdi.dll 的 BulletManager 与场景帧更新入口。 */
	constexpr std::uintptr_t kBulletUpdateRva = 0x7123B0;
	constexpr std::uintptr_t kFrameUpdateRva = 0x717130;
	constexpr std::uint8_t kBulletSignature[] = {
		0x40, 0x53, 0x48, 0x83, 0xEC, 0x50,
		0x0F, 0x29, 0x74, 0x24, 0x40, 0x0F, 0x28, 0xF1,
		0x48, 0x8B, 0xD9
	};
	constexpr std::uint8_t kFrameSignature[] = {
		0x40, 0x53, 0x56, 0x57, 0x41, 0x56, 0x48, 0x81, 0xEC,
		0xE8, 0x00, 0x00, 0x00, 0x4C, 0x89, 0xBC, 0x24,
		0xE0, 0x00, 0x00, 0x00
	};

	/* BulletManager 记录布局：视觉对象位于 +0x238，轨迹点与速度紧随其后。 */
	constexpr std::uintptr_t kRecordVisualOffset = 0x238;
	constexpr std::uintptr_t kRecordPreviousOffset = 0x30;
	constexpr std::uintptr_t kRecordCurrentOffset = 0x60;
	constexpr std::uintptr_t kRecordVelocityOffset = 0x6C;
	constexpr std::uintptr_t kRecordNextOffset = 0x78;
	constexpr std::uintptr_t kRecordFinishedOffset = 0xA8;
	constexpr std::size_t kProjectileKindCacheSize = 16;

	struct ProjectileKindEntry
	{
		std::uintptr_t vtable = 0;
		std::uint32_t kind = 0;
	};

	struct UpdateSlot
	{
		DlModProjectileUpdateCallback callback = nullptr;
		void* context = nullptr;
	};

	struct FrameSlot
	{
		DlModFrameCallback callback = nullptr;
		void* context = nullptr;
	};

	std::atomic<void*> g_bulletOriginal{ nullptr };
	std::atomic<void*> g_frameOriginal{ nullptr };
	std::atomic<bool> g_installed{ false };
	std::atomic<bool> g_frameHooked{ false };
	std::atomic<bool> g_hasUpdateCallback{ false };
	std::atomic<bool> g_hasFrameCallback{ false };
	std::mutex g_installMutex;

	/* 单写多读回调槽：写侧使用序号锁，读侧在游戏线程无锁读取。 */
	std::atomic<std::uint32_t> g_updateSequence{ 0 };
	UpdateSlot g_updateSlot{};
	std::atomic<std::uint32_t> g_frameSequence{ 0 };
	FrameSlot g_frameSlot{};

	thread_local ProjectileKindEntry g_projectileKindCache[kProjectileKindCacheSize]{};
	thread_local std::size_t g_projectileKindCursor = 0;
	thread_local std::uintptr_t g_lastVisualVtable = 0;
	thread_local std::uint32_t g_lastProjectileKind = DLMOD_PROJECTILE_KIND_UNKNOWN;

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
			value = {};
			return false;
		}
	}

	template<typename Value>
	bool WriteField(void* object, std::uintptr_t offset, const Value& value) noexcept
	{
		if (object == nullptr)
			return false;
		__try
		{
			*reinterpret_cast<Value*>(static_cast<std::uint8_t*>(object) + offset) = value;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	bool IsFinite(const DlModVector3V1& value) noexcept
	{
		return std::isfinite(value.x) && std::isfinite(value.y) &&
			std::isfinite(value.z);
	}

	void WriteUpdateSlot(
		void* context,
		DlModProjectileUpdateCallback callback) noexcept
	{
		g_updateSequence.fetch_add(1, std::memory_order_acq_rel);
		g_updateSlot.callback = callback;
		g_updateSlot.context = context;
		g_updateSequence.fetch_add(1, std::memory_order_release);
	}

	bool ReadUpdateSlot(UpdateSlot& slot) noexcept
	{
		const std::uint32_t first =
			g_updateSequence.load(std::memory_order_acquire);
		if ((first & 1U) != 0)
			return false;
		slot = g_updateSlot;
		const std::uint32_t second =
			g_updateSequence.load(std::memory_order_acquire);
		return first == second;
	}

	void WriteFrameSlot(void* context, DlModFrameCallback callback) noexcept
	{
		g_frameSequence.fetch_add(1, std::memory_order_acq_rel);
		g_frameSlot.callback = callback;
		g_frameSlot.context = context;
		g_frameSequence.fetch_add(1, std::memory_order_release);
	}

	bool ReadFrameSlot(FrameSlot& slot) noexcept
	{
		const std::uint32_t first =
			g_frameSequence.load(std::memory_order_acquire);
		if ((first & 1U) != 0)
			return false;
		slot = g_frameSlot;
		const std::uint32_t second =
			g_frameSequence.load(std::memory_order_acquire);
		return first == second;
	}

	std::uint32_t ClassifyRecord(void* record) noexcept
	{
		void* visual = nullptr;
		if (!ReadField(record, kRecordVisualOffset, visual) || visual == nullptr)
			return DLMOD_PROJECTILE_KIND_UNKNOWN;
		std::uintptr_t vtable = 0;
		if (!ReadField(visual, 0, vtable) || vtable == 0)
			return DLMOD_PROJECTILE_KIND_UNKNOWN;
		if (vtable == g_lastVisualVtable)
			return g_lastProjectileKind;
		for (const ProjectileKindEntry& entry : g_projectileKindCache)
		{
			if (entry.vtable == vtable)
			{
				g_lastVisualVtable = vtable;
				g_lastProjectileKind = entry.kind;
				return entry.kind;
			}
		}

		std::uint32_t kind = DLMOD_PROJECTILE_KIND_UNKNOWN;
		char name[192]{};
		if (EngineEntityBindings::ReadTypeName(visual, name, sizeof(name)))
		{
			/* BoltImpactBulletVis 同时包含 Bolt 与 Bullet，因此先判定 Bolt。 */
			if (std::strstr(name, "Bolt") != nullptr)
				kind = DLMOD_PROJECTILE_KIND_BOLT;
			else if (std::strstr(name, "Arrow") != nullptr)
				kind = DLMOD_PROJECTILE_KIND_ARROW;
			else if (std::strstr(name, "Bullet") != nullptr ||
				std::strstr(name, "Tracer") != nullptr)
				kind = DLMOD_PROJECTILE_KIND_BULLET;
		}

		g_projectileKindCache[g_projectileKindCursor] = { vtable, kind };
		g_projectileKindCursor = (g_projectileKindCursor + 1) %
			kProjectileKindCacheSize;
		g_lastVisualVtable = vtable;
		g_lastProjectileKind = kind;
		return kind;
	}

	void __fastcall BulletUpdateDetour(
		void* record,
		float deltaSeconds,
		std::uint8_t initialize)
	{
		const auto original = reinterpret_cast<BulletUpdateFunction>(
			g_bulletOriginal.load(std::memory_order_acquire));
		if (record != nullptr && g_hasUpdateCallback.load(std::memory_order_relaxed))
		{
			UpdateSlot slot{};
			if (ReadUpdateSlot(slot) && slot.callback != nullptr)
			{
				DlModProjectileUpdateV1 update{};
				update.structSize = sizeof(DlModProjectileUpdateV1);
				update.abiVersion = MODSDK_ABI_VERSION;
				update.kind = ClassifyRecord(record);
				update.flags = initialize != 0
					? DLMOD_PROJECTILE_UPDATE_NEW_FLIGHT
					: 0U;
				update.record = record;
				update.deltaSeconds = deltaSeconds;
				__try
				{
					slot.callback(slot.context, &update);
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
				}
			}
		}
		if (original != nullptr)
			original(record, deltaSeconds, initialize);
	}

	void __fastcall FrameUpdateDetour(void* manager)
	{
		const auto original = reinterpret_cast<FrameUpdateFunction>(
			g_frameOriginal.load(std::memory_order_acquire));
		if (original != nullptr)
			original(manager);
		if (!g_hasFrameCallback.load(std::memory_order_relaxed))
			return;
		FrameSlot slot{};
		if (!ReadFrameSlot(slot) || slot.callback == nullptr)
			return;
		__try
		{
			slot.callback(slot.context);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}
}

const DlModProjectileManagerApiV1* EngineProjectileManager::GetApi() noexcept
{
	static const DlModProjectileManagerApiV1 api = {
		sizeof(DlModProjectileManagerApiV1),
		MODSDK_ABI_VERSION,
		&Install,
		&IsInstalled,
		&SetUpdateCallback,
		&SetFrameCallback,
		&Classify,
		&ReadKinematics,
		&ReadPosition,
		&ReadVelocity,
		&WriteVelocity
	};
#if defined(_M_X64)
	return &api;
#else
	return nullptr;
#endif
}

DlModResult MODSDK_CALL EngineProjectileManager::Install() noexcept
{
#if !defined(_M_X64)
	return DLMOD_RESULT_UNSUPPORTED_FEATURE;
#else
	if (g_installed.load(std::memory_order_acquire))
		return DLMOD_RESULT_SUCCESS;
	const std::lock_guard<std::mutex> guard(g_installMutex);
	if (g_installed.load(std::memory_order_relaxed))
		return DLMOD_RESULT_SUCCESS;

	const HMODULE game = EngineEntityBindings::GameModule();
	if (!EngineEntityBindings::IsSupportedGame(game))
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;

	auto* bulletSite = reinterpret_cast<std::uint8_t*>(game) + kBulletUpdateRva;
	if (std::memcmp(
		bulletSite, kBulletSignature, sizeof(kBulletSignature)) != 0)
	{
		EngineLogger::Warning(
			"Projectile manager is unavailable: BulletManager signature mismatch.");
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	}
	const MH_STATUS initStatus = MH_Initialize();
	if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED)
		return DLMOD_RESULT_INVALID_STATE;

	void* bulletTrampoline = nullptr;
	if (MH_CreateHook(
		bulletSite,
		reinterpret_cast<void*>(&BulletUpdateDetour),
		&bulletTrampoline) != MH_OK)
	{
		return DLMOD_RESULT_INVALID_STATE;
	}
	g_bulletOriginal.store(bulletTrampoline, std::memory_order_release);
	if (MH_EnableHook(bulletSite) != MH_OK)
	{
		MH_RemoveHook(bulletSite);
		g_bulletOriginal.store(nullptr, std::memory_order_release);
		return DLMOD_RESULT_INVALID_STATE;
	}
	g_installed.store(true, std::memory_order_release);

	/* 帧 Hook 仅用于模组帧回调，缺失时不影响弹丸更新。 */
	auto* frameSite = reinterpret_cast<std::uint8_t*>(game) + kFrameUpdateRva;
	if (std::memcmp(frameSite, kFrameSignature, sizeof(kFrameSignature)) == 0)
	{
		void* frameTrampoline = nullptr;
		if (MH_CreateHook(
			frameSite,
			reinterpret_cast<void*>(&FrameUpdateDetour),
			&frameTrampoline) == MH_OK)
		{
			g_frameOriginal.store(frameTrampoline, std::memory_order_release);
			if (MH_EnableHook(frameSite) == MH_OK)
			{
				g_frameHooked.store(true, std::memory_order_release);
			}
			else
			{
				MH_RemoveHook(frameSite);
				g_frameOriginal.store(nullptr, std::memory_order_release);
			}
		}
	}

	EngineLogger::Info("Projectile manager hooks installed.");
	return DLMOD_RESULT_SUCCESS;
#endif
}

uint8_t MODSDK_CALL EngineProjectileManager::IsInstalled() noexcept
{
	return g_installed.load(std::memory_order_acquire) ? 1U : 0U;
}

void MODSDK_CALL EngineProjectileManager::SetUpdateCallback(
	void* context,
	DlModProjectileUpdateCallback callback) noexcept
{
	WriteUpdateSlot(context, callback);
	g_hasUpdateCallback.store(callback != nullptr, std::memory_order_release);
}

void MODSDK_CALL EngineProjectileManager::SetFrameCallback(
	void* context,
	DlModFrameCallback callback) noexcept
{
	WriteFrameSlot(context, callback);
	g_hasFrameCallback.store(callback != nullptr, std::memory_order_release);
}

uint32_t MODSDK_CALL EngineProjectileManager::Classify(const void* record) noexcept
{
	return ClassifyRecord(const_cast<void*>(record));
}

DlModResult MODSDK_CALL EngineProjectileManager::ReadKinematics(
	const void* record,
	DlModProjectileKinematicsV1* kinematics) noexcept
{
	if (record == nullptr || kinematics == nullptr ||
		kinematics->structSize <
			static_cast<uint32_t>(sizeof(DlModProjectileKinematicsV1)) ||
		kinematics->abiVersion != MODSDK_ABI_VERSION)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}

	DlModProjectileKinematicsV1 result{};
	result.structSize = sizeof(DlModProjectileKinematicsV1);
	result.abiVersion = MODSDK_ABI_VERSION;
	std::uint8_t finished = 0;
	if (!ReadField(record, kRecordPreviousOffset, result.previous) ||
		!ReadField(record, kRecordCurrentOffset, result.current) ||
		!ReadField(record, kRecordNextOffset, result.next) ||
		!ReadField(record, kRecordFinishedOffset, finished) ||
		!IsFinite(result.previous) || !IsFinite(result.current) ||
		!IsFinite(result.next))
	{
		return DLMOD_RESULT_INVALID_STATE;
	}
	if (finished != 0)
		result.flags |= DLMOD_PROJECTILE_KINEMATICS_FINISHED;
	result.flags |= DLMOD_PROJECTILE_KINEMATICS_HAS_NEXT;
	*kinematics = result;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineProjectileManager::ReadPosition(
	const void* record,
	DlModVector3V1* position) noexcept
{
	if (record == nullptr || position == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	DlModVector3V1 value{};
	if (!ReadField(record, kRecordCurrentOffset, value) || !IsFinite(value))
		return DLMOD_RESULT_INVALID_STATE;
	*position = value;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineProjectileManager::ReadVelocity(
	const void* record,
	DlModVector3V1* velocity) noexcept
{
	if (record == nullptr || velocity == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	DlModVector3V1 value{};
	if (!ReadField(record, kRecordVelocityOffset, value) || !IsFinite(value))
		return DLMOD_RESULT_INVALID_STATE;
	*velocity = value;
	return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL EngineProjectileManager::WriteVelocity(
	void* record,
	const DlModVector3V1* velocity) noexcept
{
	if (record == nullptr || velocity == nullptr || !IsFinite(*velocity))
		return DLMOD_RESULT_INVALID_ARGUMENT;
	return WriteField(record, kRecordVelocityOffset, *velocity)
		? DLMOD_RESULT_SUCCESS
		: DLMOD_RESULT_INVALID_STATE;
}

void EngineProjectileManager::Shutdown() noexcept
{
#if defined(_M_X64)
	const std::lock_guard<std::mutex> guard(g_installMutex);
	const HMODULE game = EngineEntityBindings::GameModule();
	if (game != nullptr)
	{
		if (g_frameHooked.exchange(false, std::memory_order_acq_rel))
		{
			void* frameSite = reinterpret_cast<std::uint8_t*>(game) +
				kFrameUpdateRva;
			MH_DisableHook(frameSite);
			MH_RemoveHook(frameSite);
		}
		if (g_installed.exchange(false, std::memory_order_acq_rel))
		{
			void* bulletSite = reinterpret_cast<std::uint8_t*>(game) +
				kBulletUpdateRva;
			MH_DisableHook(bulletSite);
			MH_RemoveHook(bulletSite);
		}
	}
	g_bulletOriginal.store(nullptr, std::memory_order_release);
	g_frameOriginal.store(nullptr, std::memory_order_release);
#endif
	WriteUpdateSlot(nullptr, nullptr);
	WriteFrameSlot(nullptr, nullptr);
	g_hasUpdateCallback.store(false, std::memory_order_release);
	g_hasFrameCallback.store(false, std::memory_order_release);
}
