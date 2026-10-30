#include "runtime/EngineHookRuntime.h"

#include <MinHook.h>

#include <mutex>
#include <string>
#include <vector>

namespace
{

	struct HookNode final
	{
		std::string modId;
		void *detour = nullptr;
		void **originalSlot = nullptr;
	};

	struct HookTarget final
	{
		void *address = nullptr;
		std::vector<HookNode> nodes;
	};

	std::mutex g_hookMutex;
	std::vector<HookTarget> g_targets;
	thread_local const std::string *g_loadingModId = nullptr;

	std::vector<HookTarget>::iterator FindTarget(void *address) noexcept
	{
		for (auto target = g_targets.begin(); target != g_targets.end(); ++target)
		{
			if (target->address == address)
			{
				return target;
			}
		}
		return g_targets.end();
	}

	std::vector<HookTarget>::iterator FindOwnerTarget(
		const std::string &modId,
		std::size_t &nodeIndex) noexcept
	{
		for (auto target = g_targets.begin(); target != g_targets.end(); ++target)
		{
			for (std::size_t index = 0; index < target->nodes.size(); ++index)
			{
				if (target->nodes[index].modId == modId)
				{
					nodeIndex = index;
					return target;
				}
			}
		}
		return g_targets.end();
	}

	bool RemoveNode(HookTarget &target, std::size_t index) noexcept
	{
		if (index >= target.nodes.size())
		{
			return false;
		}

		HookNode &node = target.nodes[index];
		void *continuation = node.originalSlot != nullptr ? *node.originalSlot : nullptr;
		if (index == 0)
		{
			if (target.nodes.size() == 1)
			{
				MH_DisableHook(target.address);
				MH_RemoveHook(target.address);
				target.nodes.erase(target.nodes.begin());
				return true;
			}

			HookNode &replacement = target.nodes[1];
			MH_DisableHook(target.address);
			MH_RemoveHook(target.address);

			void *trampoline = nullptr;
			if (MH_CreateHook(target.address, replacement.detour, &trampoline) != MH_OK)
			{
				void *restoredTrampoline = nullptr;
				if (MH_CreateHook(target.address, node.detour, &restoredTrampoline) == MH_OK)
				{
					if (node.originalSlot != nullptr)
					{
						*node.originalSlot = continuation;
					}
					if (target.nodes.back().originalSlot != nullptr)
					{
						*target.nodes.back().originalSlot = restoredTrampoline;
					}
					MH_EnableHook(target.address);
				}
				return false;
			}
			if (MH_EnableHook(target.address) != MH_OK)
			{
				MH_RemoveHook(target.address);
				void *restoredTrampoline = nullptr;
				if (MH_CreateHook(target.address, node.detour, &restoredTrampoline) == MH_OK)
				{
					if (node.originalSlot != nullptr)
					{
						*node.originalSlot = continuation;
					}
					if (target.nodes.back().originalSlot != nullptr)
					{
						*target.nodes.back().originalSlot = restoredTrampoline;
					}
					MH_EnableHook(target.address);
				}
				return false;
			}

			HookNode &tail = target.nodes.back();
			if (tail.originalSlot != nullptr)
			{
				*tail.originalSlot = trampoline;
			}
			target.nodes.erase(target.nodes.begin());
			return true;
		}

		HookNode &previous = target.nodes[index - 1];
		if (previous.originalSlot != nullptr)
		{
			*previous.originalSlot = continuation;
		}
		target.nodes.erase(target.nodes.begin() + static_cast<std::ptrdiff_t>(index));
		return true;
	}

}

const DlModHookApiV1 *EngineHookRuntime::GetApi() noexcept
{
	static const DlModHookApiV1 api = {
		sizeof(DlModHookApiV1),
		MODSDK_ABI_VERSION,
		&RegisterHook};
	return &api;
}

DlModResult EngineHookRuntime::RegisterForMod(
	const std::string &modId,
	void *targetAddress,
	void *detour,
	void **original) noexcept
{
	if (modId.empty() || targetAddress == nullptr || detour == nullptr || original == nullptr ||
		targetAddress == detour)
	{
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}

	try
	{
		const std::lock_guard<std::mutex> lock(g_hookMutex);
		auto target = FindTarget(targetAddress);
		if (target != g_targets.end())
		{
			for (const HookNode &node : target->nodes)
			{
				if (node.detour == detour || node.originalSlot == original)
				{
					return DLMOD_RESULT_INVALID_STATE;
				}
			}
			target->nodes.push_back({modId, detour, original});
			void *continuation = *target->nodes[target->nodes.size() - 2].originalSlot;
			*target->nodes[target->nodes.size() - 2].originalSlot = detour;
			*original = continuation;
			return DLMOD_RESULT_SUCCESS;
		}

		const MH_STATUS initializeStatus = MH_Initialize();
		if (initializeStatus != MH_OK && initializeStatus != MH_ERROR_ALREADY_INITIALIZED)
		{
			return DLMOD_RESULT_INVALID_STATE;
		}
		void *trampoline = nullptr;
		if (MH_CreateHook(targetAddress, detour, &trampoline) != MH_OK)
		{
			return DLMOD_RESULT_INVALID_STATE;
		}
		*original = trampoline;
		if (MH_EnableHook(targetAddress) != MH_OK)
		{
			MH_RemoveHook(targetAddress);
			*original = nullptr;
			return DLMOD_RESULT_INVALID_STATE;
		}
		try
		{
			g_targets.push_back({targetAddress, {{modId, detour, original}}});
		}
		catch (...)
		{
			MH_DisableHook(targetAddress);
			MH_RemoveHook(targetAddress);
			*original = nullptr;
			throw;
		}
		return DLMOD_RESULT_SUCCESS;
	}
	catch (...)
	{
		return DLMOD_RESULT_INVALID_STATE;
	}
}

bool EngineHookRuntime::RemoveForMod(const std::string &modId) noexcept
{
	try
	{
		const std::lock_guard<std::mutex> lock(g_hookMutex);
		for (;;)
		{
			std::size_t nodeIndex = 0;
			auto target = FindOwnerTarget(modId, nodeIndex);
			if (target == g_targets.end())
			{
				break;
			}
			if (!RemoveNode(*target, nodeIndex))
			{
				return false;
			}
			if (target->nodes.empty())
			{
				g_targets.erase(target);
			}
		}
		return true;
	}
	catch (...)
	{
		return false;
	}
}

void EngineHookRuntime::RemoveAll() noexcept
{
	try
	{
		const std::lock_guard<std::mutex> lock(g_hookMutex);
		for (HookTarget &target : g_targets)
		{
			MH_DisableHook(target.address);
			MH_RemoveHook(target.address);
			for (HookNode &node : target.nodes)
			{
				if (node.originalSlot != nullptr)
				{
					*node.originalSlot = nullptr;
				}
			}
		}
		g_targets.clear();
	}
	catch (...)
	{
	}
}

DlModResult MODSDK_CALL EngineHookRuntime::RegisterHook(
	void *target,
	void *detour,
	void **original) noexcept
{
	return g_loadingModId == nullptr
			   ? DLMOD_RESULT_INVALID_STATE
			   : RegisterForMod(*g_loadingModId, target, detour, original);
}

namespace EngineHookRuntimeDetail
{

	const std::string *SetLoadingMod(const std::string *modId) noexcept
	{
		const std::string *previous = g_loadingModId;
		g_loadingModId = modId;
		return previous;
	}

}
