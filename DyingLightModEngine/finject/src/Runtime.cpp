#include "finject/runtime.h"

#include <Windows.h>
#include <MinHook.h>

#include <mutex>
#include <unordered_set>

namespace
{
	std::mutex g_runtimeMutex;
	std::unordered_set<void*> g_targets;
}

finject::Result finject::Runtime::Resolve(const Target& target, void** address) noexcept
{
	if (address == nullptr)
	{
		return Result::InvalidArgument;
	}
	*address = nullptr;
	if (target.kind == TargetKind::AbsoluteAddress)
	{
		if (target.address == nullptr)
		{
			return Result::InvalidArgument;
		}
		*address = target.address;
		return Result::Success;
	}
	if (target.kind != TargetKind::ModuleOffset || target.moduleName.empty())
	{
		return Result::InvalidArgument;
	}

	const HMODULE module = GetModuleHandleW(target.moduleName.c_str());
	if (module == nullptr)
	{
		return Result::ModuleNotLoaded;
	}
	const auto base = reinterpret_cast<std::uintptr_t>(module);
	if (target.offset > static_cast<std::uintptr_t>(-1) - base)
	{
		return Result::InvalidArgument;
	}
	*address = reinterpret_cast<void*>(base + target.offset);
	return Result::Success;
}

finject::Result finject::Runtime::Install(
	void* target,
	void* detour,
	OriginalPublisher publishOriginal,
	void* context) noexcept
{
	if (target == nullptr || detour == nullptr || publishOriginal == nullptr || target == detour)
	{
		return Result::InvalidArgument;
	}

	try
	{
		const std::lock_guard<std::mutex> lock(g_runtimeMutex);
		if (g_targets.find(target) != g_targets.end())
		{
			return Result::AlreadyRegistered;
		}
		const MH_STATUS initializeStatus = MH_Initialize();
		if (initializeStatus != MH_OK && initializeStatus != MH_ERROR_ALREADY_INITIALIZED)
		{
			return Result::HookOperationFailed;
		}
		const auto registration = g_targets.insert(target);
		if (!registration.second)
		{
			return Result::AlreadyRegistered;
		}

		void* trampoline = nullptr;
		if (MH_CreateHook(target, detour, &trampoline) != MH_OK)
		{
			g_targets.erase(registration.first);
			return Result::HookOperationFailed;
		}
		publishOriginal(trampoline, context);
		if (MH_EnableHook(target) != MH_OK)
		{
			MH_RemoveHook(target);
			publishOriginal(nullptr, context);
			g_targets.erase(registration.first);
			return Result::HookOperationFailed;
		}
		return Result::Success;
	}
	catch (...)
	{
		return Result::HookOperationFailed;
	}
}

finject::Result finject::Runtime::Remove(void* target) noexcept
{
	if (target == nullptr)
	{
		return Result::InvalidArgument;
	}
	try
	{
		const std::lock_guard<std::mutex> lock(g_runtimeMutex);
		const auto registered = g_targets.find(target);
		if (registered == g_targets.end())
		{
			return Result::InvalidState;
		}
		if (MH_DisableHook(target) != MH_OK)
		{
			return Result::HookOperationFailed;
		}
		if (MH_RemoveHook(target) != MH_OK)
		{
			return Result::HookOperationFailed;
		}
		g_targets.erase(registered);
		return Result::Success;
	}
	catch (...)
	{
		return Result::HookOperationFailed;
	}
}
