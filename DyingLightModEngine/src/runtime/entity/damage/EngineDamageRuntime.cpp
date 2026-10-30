#include "runtime/entity/damage/EngineDamageRuntime.h"

#include "logging/EngineLogger.h"
#include "runtime/EngineEntityBindings.h"
#include "runtime/EngineEntityRuntime.h"

#include <Windows.h>
#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>

extern "C"
{
	void *g_engine_entity_damage_outer_trampoline = nullptr;
	void *g_engine_entity_damage_base_trampoline = nullptr;
	void EngineEntityDamageOuterStub();
	void EngineEntityDamageBaseStub();
}

namespace
{
	/* 两个入口的序言：都是 mov rax,rsp + 保存 rcx/rdx + 把 xmm2(伤害) 落到 home 槽。 */
	constexpr std::uint8_t kArmorDamageSignature[] = {
		0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0xF3,
		0x0F, 0x11, 0x50, 0x18, 0x55, 0x56, 0x57, 0x41};
	constexpr std::uint8_t kHealthDamageSignature[] = {
		0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x48,
		0x89, 0x48, 0x08, 0x55, 0x56, 0x57, 0x41, 0x57};
	constexpr std::size_t kMaxDamageCallbacks = 8;
	constexpr std::size_t kMaxDamageDepth = 8;
	constexpr std::uint64_t kProceedBit = 1ULL << 32;

	struct DamageCallback
	{
		ModEngineDamagePreHookFn pre = nullptr;
		ModEngineDamagePostHookFn post = nullptr;
	};

	struct DamageCall
	{
		bool outer = false;
		bool applyPass = false;
	};

	struct DamageFrame
	{
		void *module = nullptr;
		void *entity = nullptr;
		void *instigator = nullptr;
		float incomingDamage = 0.0F;
		float damage = 0.0F;
		float appliedDamage = 0.0F;
		float healthBefore = 0.0F;
		float healthAfter = 0.0F;
		std::uint32_t damageType = 0;
		std::uint32_t flags = 0;
		std::uint64_t sequence = 0;
		bool hasApplied = false;
		bool active = false;
	};

	SRWLOCK g_callbackLock = SRWLOCK_INIT;
	DamageCallback g_callbacks[kMaxDamageCallbacks]{};
	std::size_t g_callbackCount = 0;
	std::atomic<bool> g_installed{false};
	std::atomic<std::uint64_t> g_sequence{0};
	std::mutex g_installMutex;

	/* 受伤会递归转调（Armor/BodyParts 覆写内部两次调用基类），
	   用线程局部深度保证一次受伤事件只对外回调一次。 */
	thread_local DamageCall t_calls[kMaxDamageDepth];
	thread_local std::size_t t_callDepth = 0;
	thread_local DamageFrame t_frame;
	thread_local DamageCallback t_callbacks[kMaxDamageCallbacks];
	thread_local std::size_t t_callbackCount = 0;

	float BitsToFloat(std::uint32_t bits) noexcept
	{
		float value = 0.0F;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	}

	std::uint32_t FloatToBits(float value) noexcept
	{
		std::uint32_t bits = 0;
		std::memcpy(&bits, &value, sizeof(bits));
		return bits;
	}

	float SanitizeDamage(float value, float fallback) noexcept
	{
		return std::isfinite(value) ? value : fallback;
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

	bool IsInsideGameModule(const void *pointer) noexcept
	{
		const HMODULE game = EngineEntityBindings::GameModule();
		if (game == nullptr || pointer == nullptr)
			return false;
		MEMORY_BASIC_INFORMATION info{};
		if (VirtualQuery(pointer, &info, sizeof(info)) == 0)
			return false;
		return info.AllocationBase == static_cast<const void *>(game);
	}

	/* CHealthModule 由 AIModule 派生，+0x40 保存宿主实体基址。 */
	void *ReadOwnerEntity(const void *module) noexcept
	{
		void *owner = nullptr;
		if (!ReadField(module, EngineEntityBindings::HealthModuleOwnerOffset, owner) ||
			owner == nullptr)
		{
			return nullptr;
		}
		std::uintptr_t vtable = 0;
		if (!ReadField(owner, 0, vtable) || vtable == 0 ||
			!IsInsideGameModule(reinterpret_cast<const void *>(vtable)))
		{
			return nullptr;
		}
		return owner;
	}

	float ReadModuleHealth(const void *module) noexcept
	{
		float health = 0.0F;
		if (!ReadField(module, EngineEntityBindings::HealthBodyOffset, health) ||
			!std::isfinite(health))
		{
			return 0.0F;
		}
		return health;
	}

	void CopyCallbacks() noexcept
	{
		AcquireSRWLockShared(&g_callbackLock);
		t_callbackCount = g_callbackCount;
		for (std::size_t index = 0; index < g_callbackCount; ++index)
		{
			t_callbacks[index] = g_callbacks[index];
		}
		ReleaseSRWLockShared(&g_callbackLock);
	}

	void BuildEvent(
		DlModDamageEventV1 &event,
		const DamageFrame &frame,
		std::uint32_t stageFlags) noexcept
	{
		event.structSize = sizeof(DlModDamageEventV1);
		event.abiVersion = MODSDK_ABI_VERSION;
		event.flags = stageFlags | frame.flags;
		event.damageType = frame.damageType;
		event.healthModule = frame.module;
		event.entity = frame.entity;
		event.instigator = frame.instigator;
		event.incomingDamage = frame.incomingDamage;
		event.damage = frame.damage;
		event.appliedDamage = frame.appliedDamage;
		event.healthBefore = frame.healthBefore;
		event.healthAfter = frame.healthAfter;
		event.sequence = frame.sequence;
	}

	void FirePostCallbacks(const DlModDamageEventV1 &event) noexcept
	{
		for (std::size_t index = 0; index < t_callbackCount; ++index)
		{
			ModEngineDamagePostHookFn post = t_callbacks[index].post;
			if (post == nullptr)
				continue;
			try
			{
				post(&event);
			}
			catch (...)
			{
			}
		}
	}
}

/* 入口回调：返回 (允许结算 << 32) | 伤害位。阻止时调用方直接返回 0。 */
extern "C" std::uint64_t EngineEntity_OnDamageEntry(
	void *module,
	void *instigator,
	std::uint32_t damageBits,
	std::uint32_t damageType,
	std::uint32_t skipHealth) noexcept
{
	const float incoming = BitsToFloat(damageBits);
	const bool outer = t_callDepth == 0;
	if (t_callDepth < kMaxDamageDepth)
	{
		t_calls[t_callDepth].outer = outer;
		/* 基类第 11 个参数非 0 表示只计算伤害、不扣除生命。 */
		t_calls[t_callDepth].applyPass = skipHealth == 0;
	}
	++t_callDepth;

	if (!g_installed.load(std::memory_order_acquire) || module == nullptr ||
		!std::isfinite(incoming))
	{
		return kProceedBit | damageBits;
	}
	if (!outer)
	{
		/* 覆写入口内部的再次转调：只透传，避免同一次受伤重复回调。 */
		return kProceedBit | damageBits;
	}

	DamageFrame &frame = t_frame;
	frame.module = module;
	frame.entity = ReadOwnerEntity(module);
	frame.instigator = instigator != module ? instigator : nullptr;
	frame.incomingDamage = incoming;
	frame.damage = incoming;
	frame.appliedDamage = 0.0F;
	frame.healthBefore = ReadModuleHealth(module);
	frame.healthAfter = frame.healthBefore;
	frame.damageType = damageType;
	frame.sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
	frame.flags = 0;
	if (frame.entity != nullptr &&
		EngineEntityRuntime::IsPlayerInstance(frame.entity))
	{
		frame.flags |= DLMOD_DAMAGE_FLAG_VICTIM_PLAYER;
	}
	frame.hasApplied = false;
	frame.active = true;
	CopyCallbacks();

	DlModDamageEventV1 event{};
	BuildEvent(event, frame, DLMOD_DAMAGE_FLAG_PRE);
	bool allow = true;
	for (std::size_t index = 0; index < t_callbackCount; ++index)
	{
		ModEngineDamagePreHookFn pre = t_callbacks[index].pre;
		if (pre == nullptr)
			continue;
		try
		{
			if (pre(&event) == 0)
				allow = false;
		}
		catch (...)
		{
		}
	}

	const float adjusted = SanitizeDamage(event.damage, incoming);
	frame.damage = adjusted;
	if (!allow)
	{
		frame.damage = 0.0F;
		frame.appliedDamage = 0.0F;
		BuildEvent(
			event,
			frame,
			DLMOD_DAMAGE_FLAG_POST | DLMOD_DAMAGE_FLAG_BLOCKED);
		frame.active = false;
		--t_callDepth;
		FirePostCallbacks(event);
		return 0;
	}
	return kProceedBit | FloatToBits(adjusted);
}

/* 返回回调：嵌套转调记录游戏结算值，最外层触发受伤后回调。 */
extern "C" void EngineEntity_OnDamageReturn(std::uint32_t returnBits) noexcept
{
	if (t_callDepth == 0)
		return;
	--t_callDepth;
	if (t_callDepth >= kMaxDamageDepth)
		return;

	const bool outer = t_calls[t_callDepth].outer;
	const bool applyPass = t_calls[t_callDepth].applyPass;
	const float applied = BitsToFloat(returnBits);
	if (!outer)
	{
		if (t_frame.active && applyPass)
		{
			const float value = std::isfinite(applied) ? applied : 0.0F;
			t_frame.appliedDamage = t_frame.hasApplied
										? (std::max)(t_frame.appliedDamage, value)
										: value;
			t_frame.hasApplied = true;
		}
		return;
	}
	if (!t_frame.active)
		return;

	t_frame.active = false;
	const float value = t_frame.hasApplied
							? t_frame.appliedDamage
							: (std::isfinite(applied) ? applied : 0.0F);
	t_frame.appliedDamage = value;
	t_frame.healthAfter = ReadModuleHealth(t_frame.module);
	DlModDamageEventV1 event{};
	BuildEvent(
		event,
		t_frame,
		DLMOD_DAMAGE_FLAG_POST | DLMOD_DAMAGE_FLAG_GAME_APPLIED);
	FirePostCallbacks(event);
}

bool EngineDamageRuntime::Install() noexcept
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

	void *armorSite = reinterpret_cast<std::uint8_t *>(game) +
					  EngineEntityBindings::HealthModuleArmorDamageSiteRva;
	void *baseSite = reinterpret_cast<std::uint8_t *>(game) +
					 EngineEntityBindings::HealthModuleDamageSiteRva;
	if (std::memcmp(
			armorSite, kArmorDamageSignature, sizeof(kArmorDamageSignature)) != 0 ||
		std::memcmp(
			baseSite, kHealthDamageSignature, sizeof(kHealthDamageSignature)) != 0)
	{
		EngineLogger::Warning(
			"Damage hook sites did not match the expected game build");
		return false;
	}

	const MH_STATUS initStatus = MH_Initialize();
	if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED)
		return false;

	void *armorTrampoline = nullptr;
	if (MH_CreateHook(
			armorSite,
			reinterpret_cast<void *>(&EngineEntityDamageOuterStub),
			&armorTrampoline) != MH_OK)
	{
		EngineLogger::Warning("Damage hook for the armor health module failed");
		return false;
	}
	void *baseTrampoline = nullptr;
	if (MH_CreateHook(
			baseSite,
			reinterpret_cast<void *>(&EngineEntityDamageBaseStub),
			&baseTrampoline) != MH_OK)
	{
		MH_RemoveHook(armorSite);
		EngineLogger::Warning("Damage hook for the base health module failed");
		return false;
	}

	g_engine_entity_damage_outer_trampoline = armorTrampoline;
	g_engine_entity_damage_base_trampoline = baseTrampoline;
	if (MH_EnableHook(armorSite) != MH_OK || MH_EnableHook(baseSite) != MH_OK)
	{
		MH_DisableHook(armorSite);
		MH_DisableHook(baseSite);
		MH_RemoveHook(armorSite);
		MH_RemoveHook(baseSite);
		g_engine_entity_damage_outer_trampoline = nullptr;
		g_engine_entity_damage_base_trampoline = nullptr;
		EngineLogger::Warning("Damage hooks could not be enabled");
		return false;
	}

	g_installed.store(true, std::memory_order_release);
	return true;
#endif
}

bool EngineDamageRuntime::IsInstalled() noexcept
{
	return g_installed.load(std::memory_order_acquire);
}

void EngineDamageRuntime::Shutdown() noexcept
{
#if defined(_M_X64)
	const std::lock_guard<std::mutex> guard(g_installMutex);
	if (g_installed.load(std::memory_order_acquire))
	{
		const HMODULE game = EngineEntityBindings::GameModule();
		if (game != nullptr)
		{
			void *armorSite = reinterpret_cast<std::uint8_t *>(game) +
							  EngineEntityBindings::HealthModuleArmorDamageSiteRva;
			void *baseSite = reinterpret_cast<std::uint8_t *>(game) +
							 EngineEntityBindings::HealthModuleDamageSiteRva;
			MH_DisableHook(armorSite);
			MH_DisableHook(baseSite);
			MH_RemoveHook(armorSite);
			MH_RemoveHook(baseSite);
		}
	}
#endif
	g_installed.store(false, std::memory_order_release);
	g_engine_entity_damage_outer_trampoline = nullptr;
	g_engine_entity_damage_base_trampoline = nullptr;
	AcquireSRWLockExclusive(&g_callbackLock);
	g_callbackCount = 0;
	for (DamageCallback &callback : g_callbacks)
	{
		callback = {};
	}
	ReleaseSRWLockExclusive(&g_callbackLock);
	t_callDepth = 0;
	t_frame = {};
}

DlModResult EngineDamageRuntime::RegisterHook(
	ModEngineDamagePreHookFn pre,
	ModEngineDamagePostHookFn post) noexcept
{
	if (pre == nullptr && post == nullptr)
		return DLMOD_RESULT_INVALID_ARGUMENT;
	const std::lock_guard<std::mutex> guard(g_installMutex);
	AcquireSRWLockExclusive(&g_callbackLock);
	for (std::size_t index = 0; index < g_callbackCount; ++index)
	{
		if (g_callbacks[index].pre == pre && g_callbacks[index].post == post)
		{
			ReleaseSRWLockExclusive(&g_callbackLock);
			return DLMOD_RESULT_INVALID_STATE;
		}
	}
	if (g_callbackCount >= kMaxDamageCallbacks)
	{
		ReleaseSRWLockExclusive(&g_callbackLock);
		return DLMOD_RESULT_INVALID_STATE;
	}
	g_callbacks[g_callbackCount].pre = pre;
	g_callbacks[g_callbackCount].post = post;
	++g_callbackCount;
	ReleaseSRWLockExclusive(&g_callbackLock);
	return DLMOD_RESULT_SUCCESS;
}

DlModResult EngineDamageRuntime::UnregisterHook(
	ModEngineDamagePreHookFn pre,
	ModEngineDamagePostHookFn post) noexcept
{
	const std::lock_guard<std::mutex> guard(g_installMutex);
	AcquireSRWLockExclusive(&g_callbackLock);
	for (std::size_t index = 0; index < g_callbackCount; ++index)
	{
		if (g_callbacks[index].pre != pre || g_callbacks[index].post != post)
			continue;
		for (std::size_t move = index + 1; move < g_callbackCount; ++move)
		{
			g_callbacks[move - 1] = g_callbacks[move];
		}
		--g_callbackCount;
		g_callbacks[g_callbackCount] = {};
		ReleaseSRWLockExclusive(&g_callbackLock);
		return DLMOD_RESULT_SUCCESS;
	}
	ReleaseSRWLockExclusive(&g_callbackLock);
	return DLMOD_RESULT_NOT_FOUND;
}
