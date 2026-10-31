#include "tracking/EngineHunterHudBindings.h"

#include "runtime/EngineEntityBindings.h"

#include <atomic>
#include <cstring>
#include <mutex>

/* 原生 HUD 绑定实现概览：按当前受支持的游戏构建解析 IUIElement 导入槽。 */
namespace
{
	constexpr std::uintptr_t kUiElementCloneImportRva = 0x137C520;
	constexpr std::uintptr_t kUiElementGetParentImportRva = 0x137C4C8;
	constexpr std::uintptr_t kUiElementGetPositionImportRva = 0x137C608;
	constexpr std::uintptr_t kUiElementGetUnscaledSizeImportRva = 0x137C920;
	constexpr std::uintptr_t kUiElementSetPositionImportRva = 0x137CA50;
	constexpr std::uintptr_t kUiElementSetScaleImportRva = 0x137C7B0;
	constexpr std::uintptr_t kUiElementSetColorImportRva = 0x137C4F8;
	constexpr std::uintptr_t kUiElementSetVisibleImportRva = 0x137C500;
	constexpr std::uintptr_t kUiElementDestroyRecursiveImportRva = 0x137D4C0;
	constexpr std::uint8_t kHudInfoUpdateSignature[] = {
		0x48, 0x89, 0x5C, 0x24, 0x20, 0x55, 0x56, 0x57,
		0x48, 0x8B, 0xEC, 0x48, 0x83, 0xEC, 0x70};

	template <typename Function>
	Function ResolveImport(HMODULE module, std::uintptr_t rva) noexcept
	{
		if (module == nullptr)
			return nullptr;
		__try
		{
			return *reinterpret_cast<Function*>(
				reinterpret_cast<std::uintptr_t>(module) + rva);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
	}
}

const EngineHunterHudBindings* EngineHunterHudBindings::Resolve() noexcept
{
	static EngineHunterHudBindings bindings;
	static std::atomic_bool resolved = false;
	static std::mutex resolveMutex;
	if (resolved.load(std::memory_order_acquire))
		return &bindings;
	const std::lock_guard<std::mutex> guard(resolveMutex);
	if (resolved.load(std::memory_order_relaxed))
		return &bindings;

	const HMODULE game = EngineEntityBindings::GameModule();
	if (!EngineEntityBindings::IsSupportedGame(game))
		return nullptr;

	bindings.gameModule_ = game;
	bindings.cloneElement_ =
		ResolveImport<CloneElementFunction>(game, kUiElementCloneImportRva);
	bindings.getParent_ =
		ResolveImport<GetParentFunction>(game, kUiElementGetParentImportRva);
	bindings.getPosition_ =
		ResolveImport<GetVectorFunction>(game, kUiElementGetPositionImportRva);
	bindings.getUnscaledSize_ =
		ResolveImport<GetVectorFunction>(game, kUiElementGetUnscaledSizeImportRva);
	bindings.setPosition_ =
		ResolveImport<SetVectorFunction>(game, kUiElementSetPositionImportRva);
	bindings.setScale_ =
		ResolveImport<SetVectorFunction>(game, kUiElementSetScaleImportRva);
	bindings.setColor_ =
		ResolveImport<SetVectorFunction>(game, kUiElementSetColorImportRva);
	bindings.setVisible_ =
		ResolveImport<SetVisibleFunction>(game, kUiElementSetVisibleImportRva);
	bindings.destroyRecursive_ =
		ResolveImport<DestroyRecursiveFunction>(
			game,
			kUiElementDestroyRecursiveImportRva);

	if (bindings.cloneElement_ == nullptr ||
		bindings.getParent_ == nullptr ||
		bindings.getPosition_ == nullptr ||
		bindings.getUnscaledSize_ == nullptr ||
		bindings.setPosition_ == nullptr ||
		bindings.setScale_ == nullptr ||
		bindings.setColor_ == nullptr ||
		bindings.setVisible_ == nullptr ||
		bindings.destroyRecursive_ == nullptr)
	{
		bindings = {};
		return nullptr;
	}

	resolved.store(true, std::memory_order_release);
	return &bindings;
}

void* EngineHunterHudBindings::HudInfoUpdateSite() const noexcept
{
	if (gameModule_ == nullptr)
		return nullptr;
	return reinterpret_cast<std::uint8_t*>(gameModule_) + HudInfoUpdateSiteRva;
}

bool EngineHunterHudBindings::HasExpectedHudInfoUpdateSignature() const noexcept
{
	const void* site = HudInfoUpdateSite();
	if (site == nullptr)
		return false;
	__try
	{
		return std::memcmp(
				   site,
				   kHudInfoUpdateSignature,
				   sizeof(kHudInfoUpdateSignature)) == 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

void EngineHunterHudBindings::DestroyRecursive(void* element) const noexcept
{
	if (element == nullptr || destroyRecursive_ == nullptr)
		return;
	__try
	{
		destroyRecursive_(element);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

void* EngineHunterHudBindings::CloneElement(
	void* source,
	void* parent) const noexcept
{
	if (source == nullptr || parent == nullptr || cloneElement_ == nullptr)
		return nullptr;
	__try
	{
		return cloneElement_(source, parent);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return nullptr;
	}
}

void* EngineHunterHudBindings::GetParent(void* element) const noexcept
{
	if (element == nullptr || getParent_ == nullptr)
		return nullptr;
	__try
	{
		return getParent_(element);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return nullptr;
	}
}

bool EngineHunterHudBindings::GetPosition(
	void* element,
	Vector3& position) const noexcept
{
	position = {};
	if (element == nullptr || getPosition_ == nullptr)
		return false;
	__try
	{
		getPosition_(element, &position);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		position = {};
		return false;
	}
}

bool EngineHunterHudBindings::GetUnscaledSize(
	void* element,
	Vector3& size) const noexcept
{
	size = {};
	if (element == nullptr || getUnscaledSize_ == nullptr)
		return false;
	__try
	{
		getUnscaledSize_(element, &size);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		size = {};
		return false;
	}
}

void EngineHunterHudBindings::SetPosition(
	void* element,
	const Vector3& position) const noexcept
{
	if (element == nullptr || setPosition_ == nullptr)
		return;
	__try
	{
		setPosition_(element, &position);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

void EngineHunterHudBindings::SetScale(
	void* element,
	const Vector3& scale) const noexcept
{
	if (element == nullptr || setScale_ == nullptr)
		return;
	__try
	{
		setScale_(element, &scale);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

void EngineHunterHudBindings::SetColor(
	void* element,
	const Vector3& color) const noexcept
{
	if (element == nullptr || setColor_ == nullptr)
		return;
	__try
	{
		setColor_(element, &color);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

void EngineHunterHudBindings::SetVisible(
	void* element,
	bool visible) const noexcept
{
	if (element == nullptr || setVisible_ == nullptr)
		return;
	__try
	{
		setVisible_(element, visible);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

