#include "tracking/EngineHunterTrackingRuntime.h"

#include "runtime/EngineEntityBindings.h"
#include "tracking/EngineHunterMotionTracker.h"

#include <Windows.h>
#include <MinHook.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>

extern "C"
{
	void *g_engine_entity_hunter_position_trampoline = nullptr;
	void *g_engine_entity_hunter_movement_trampoline = nullptr;
	void EngineEntityHunterPositionStub();
	void EngineEntityHunterMovementStub();
}

namespace
{
	constexpr std::uintptr_t kPositionSiteRva = 0xECD8C7;
	constexpr std::uintptr_t kMovementSiteRva = 0x10B0461;
	constexpr std::uint8_t kPositionSignature[] = {
		0xF3, 0x0F, 0x11, 0x8F, 0xF4, 0x07, 0x00, 0x00, 0x48};
	constexpr std::uint8_t kMovementSignature[] = {
		0xF3, 0x0F, 0x11, 0x93, 0xA0, 0x01, 0x00, 0x00, 0x48};

	struct MotionState
	{
		void *controlObject = nullptr;
		void *movementObject = nullptr;
		EngineHunterMotionTracker tracker;
	};

	SRWLOCK g_stateLock = SRWLOCK_INIT;
	MotionState g_state{};
	std::atomic<void *> g_currentHunter{nullptr};
	std::atomic_bool g_installed = false;
	std::mutex g_installMutex;
	LARGE_INTEGER g_counterFrequency{};
	INIT_ONCE g_counterFrequencyOnce = INIT_ONCE_STATIC_INIT;

	BOOL CALLBACK InitializeCounterFrequency(PINIT_ONCE, PVOID, PVOID *)
	{
		return QueryPerformanceFrequency(&g_counterFrequency) &&
			   g_counterFrequency.QuadPart > 0;
	}

	bool ReadTimeSeconds(double &seconds) noexcept
	{
		seconds = 0.0;
		if (!InitOnceExecuteOnce(
				&g_counterFrequencyOnce, InitializeCounterFrequency, nullptr, nullptr))
		{
			return false;
		}
		LARGE_INTEGER counter{};
		if (!QueryPerformanceCounter(&counter))
			return false;
		seconds = static_cast<double>(counter.QuadPart) /
				  static_cast<double>(g_counterFrequency.QuadPart);
		return std::isfinite(seconds) && seconds > 0.0;
	}

	template <typename Value>
	bool ReadField(const void *object, std::uintptr_t offset, Value &value) noexcept
	{
		value = {};
		if (object == nullptr)
			return false;
		__try
		{
			value = *reinterpret_cast<const Value *>(
				static_cast<const std::uint8_t *>(object) + offset);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			value = {};
			return false;
		}
	}

	bool IsFinite(const DlModVector3V1 &value) noexcept
	{
		return std::isfinite(value.x) && std::isfinite(value.y) &&
			   std::isfinite(value.z);
	}

	void OnPosition(void *controlObject) noexcept
	{
		if (controlObject == nullptr)
			return;
		std::uint8_t modelId = 0;
		if (!ReadField(controlObject, EngineEntityBindings::HunterModelIdOffset, modelId) ||
			modelId != EngineEntityBindings::HunterModelId)
		{
			void *expected = controlObject;
			if (g_currentHunter.compare_exchange_strong(
					expected, nullptr, std::memory_order_acq_rel))
			{
				AcquireSRWLockExclusive(&g_stateLock);
				g_state = {};
				ReleaseSRWLockExclusive(&g_stateLock);
			}
			return;
		}

		const EngineEntityBindings *bindings = EngineEntityBindings::Resolve();
		DlModVector3V1 position{};
		double now = 0.0;
		if (bindings == nullptr || !ReadTimeSeconds(now) ||
			!ReadField(controlObject, EngineEntityBindings::HunterPositionOffset, position) ||
			!IsFinite(position))
		{
			return;
		}

		g_currentHunter.store(controlObject, std::memory_order_release);
		AcquireSRWLockExclusive(&g_stateLock);
		if (g_state.controlObject != controlObject)
		{
			g_state = {};
			g_state.controlObject = controlObject;
		}
		g_state.tracker.ObservePosition(position, now);
		ReleaseSRWLockExclusive(&g_stateLock);

		if (bindings->getBonePosition == nullptr ||
			bindings->resolveBone == nullptr)
		{
			return;
		}
		DlModVector3V1 skeleton{};
		__try
		{
			if (bindings->resolveBone(controlObject, 1) >= 0 &&
				bindings->getBonePosition(controlObject, &skeleton, 1) == &skeleton &&
				IsFinite(skeleton))
			{
				AcquireSRWLockExclusive(&g_stateLock);
				if (g_state.controlObject == controlObject)
					g_state.tracker.SetSkeletonPosition(skeleton, now);
				ReleaseSRWLockExclusive(&g_stateLock);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	void OnMovement(void *movementObject) noexcept
	{
		std::uint32_t marker = 0;
		DlModVector3V1 velocity{};
		if (!ReadField(
				movementObject,
				EngineEntityBindings::HunterMovementMarkerOffset,
				marker) ||
			marker != EngineEntityBindings::HunterMovementMarker ||
			!ReadField(
				movementObject,
				EngineEntityBindings::HunterMovementVelocityOffset,
				velocity) ||
			!IsFinite(velocity))
		{
			return;
		}
		void *controlObject = g_currentHunter.load(std::memory_order_acquire);
		double now = 0.0;
		if (controlObject == nullptr || !ReadTimeSeconds(now))
			return;

		AcquireSRWLockExclusive(&g_stateLock);
		if (g_state.controlObject != controlObject ||
			(g_state.movementObject != nullptr &&
			 g_state.movementObject != movementObject))
		{
			g_state = {};
			g_state.controlObject = controlObject;
		}
		g_state.movementObject = movementObject;
		g_state.tracker.ObserveVelocity(velocity, now);
		ReleaseSRWLockExclusive(&g_stateLock);
	}
}

extern "C" void EngineEntity_OnHunterPosition(void *controlObject)
{
	OnPosition(controlObject);
}

extern "C" void EngineEntity_OnHunterMovement(void *movementObject)
{
	OnMovement(movementObject);
}

bool EngineHunterTrackingRuntime::Install() noexcept
{
#if !defined(_M_X64)
	return false;
#else
	if (g_installed.load(std::memory_order_acquire))
		return true;
	const std::lock_guard<std::mutex> guard(g_installMutex);
	if (g_installed.load(std::memory_order_relaxed))
		return true;
	const HMODULE game = EngineEntityBindings::GameModule();
	if (!EngineEntityBindings::IsSupportedGame(game))
		return false;

	auto *positionSite = reinterpret_cast<std::uint8_t *>(game) +
						 kPositionSiteRva;
	auto *movementSite = reinterpret_cast<std::uint8_t *>(game) +
						 kMovementSiteRva;
	if (std::memcmp(positionSite, kPositionSignature, sizeof(kPositionSignature)) != 0 ||
		std::memcmp(movementSite, kMovementSignature, sizeof(kMovementSignature)) != 0)
	{
		return false;
	}
	const MH_STATUS initStatus = MH_Initialize();
	if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED)
		return false;

	void *positionTrampoline = nullptr;
	void *movementTrampoline = nullptr;
	if (MH_CreateHook(
			positionSite,
			reinterpret_cast<void *>(&EngineEntityHunterPositionStub),
			&positionTrampoline) != MH_OK)
	{
		return false;
	}
	if (MH_CreateHook(
			movementSite,
			reinterpret_cast<void *>(&EngineEntityHunterMovementStub),
			&movementTrampoline) != MH_OK)
	{
		MH_RemoveHook(positionSite);
		return false;
	}

	g_engine_entity_hunter_position_trampoline = positionTrampoline;
	g_engine_entity_hunter_movement_trampoline = movementTrampoline;
	if (MH_EnableHook(positionSite) != MH_OK)
	{
		MH_RemoveHook(positionSite);
		MH_RemoveHook(movementSite);
		g_engine_entity_hunter_position_trampoline = nullptr;
		g_engine_entity_hunter_movement_trampoline = nullptr;
		return false;
	}
	if (MH_EnableHook(movementSite) != MH_OK)
	{
		MH_DisableHook(positionSite);
		MH_RemoveHook(positionSite);
		MH_RemoveHook(movementSite);
		g_engine_entity_hunter_position_trampoline = nullptr;
		g_engine_entity_hunter_movement_trampoline = nullptr;
		return false;
	}
	g_installed.store(true, std::memory_order_release);
	return true;
#endif
}

bool EngineHunterTrackingRuntime::IsInstalled() noexcept
{
	return g_installed.load(std::memory_order_acquire);
}

void EngineHunterTrackingRuntime::Shutdown() noexcept
{
#if defined(_M_X64)
	const std::lock_guard<std::mutex> guard(g_installMutex);
	if (g_installed.exchange(false, std::memory_order_acq_rel))
	{
		const HMODULE game = EngineEntityBindings::GameModule();
		if (game != nullptr)
		{
			void *positionSite = reinterpret_cast<std::uint8_t *>(game) +
								 kPositionSiteRva;
			void *movementSite = reinterpret_cast<std::uint8_t *>(game) +
								 kMovementSiteRva;
			MH_DisableHook(positionSite);
			MH_DisableHook(movementSite);
			MH_RemoveHook(positionSite);
			MH_RemoveHook(movementSite);
		}
	}
	g_engine_entity_hunter_position_trampoline = nullptr;
	g_engine_entity_hunter_movement_trampoline = nullptr;
	g_currentHunter.store(nullptr, std::memory_order_release);
	AcquireSRWLockExclusive(&g_stateLock);
	g_state = {};
	ReleaseSRWLockExclusive(&g_stateLock);
#endif
}

void *EngineHunterTrackingRuntime::GetCurrentControlObject() noexcept
{
	return g_currentHunter.load(std::memory_order_acquire);
}

bool EngineHunterTrackingRuntime::ReadMotion(
	void *controlObject,
	DlModHunterMotionV1 &motion) noexcept
{
	motion = {};
	motion.structSize = sizeof(motion);
	motion.abiVersion = MODSDK_ABI_VERSION;
	double now = 0.0;
	if (controlObject == nullptr || !ReadTimeSeconds(now))
		return false;
	AcquireSRWLockShared(&g_stateLock);
	const bool matches =
		g_currentHunter.load(std::memory_order_acquire) == controlObject &&
		g_state.controlObject == controlObject;
	const bool read = matches && g_state.tracker.Read(motion, now);
	ReleaseSRWLockShared(&g_stateLock);
	return read;
}
