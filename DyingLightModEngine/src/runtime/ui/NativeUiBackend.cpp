#include "runtime/ui/NativeUiBackend.h"

namespace dl::engineui
{

	/* 生命周期概览：原生后端只保存解析器，不拥有游戏 UI 根对象。 */
	NativeUiBackend::~NativeUiBackend()
	{
		Shutdown();
	}

	bool NativeUiBackend::Initialize(const NativeUiResolver &resolver) noexcept
	{
		if (initialized_ || resolver.getVisibleRoot == nullptr ||
			resolver.attach == nullptr || resolver.destroyRecursive == nullptr)
		{
			return false;
		}
		resolver_ = resolver;
		initialized_ = true;
		return true;
	}

	void NativeUiBackend::Shutdown() noexcept
	{
		resolver_ = {};
		initialized_ = false;
	}

	bool NativeUiBackend::IsInitialized() const noexcept
	{
		return initialized_;
	}

	UiHandle NativeUiBackend::CreateRoot() noexcept
	{
		if (!HasResolver())
		{
			return 0;
		}
		return resolver_.getVisibleRoot(resolver_.context);
	}

	UiHandle NativeUiBackend::FindElement(UiHandle parent, const char *id) const noexcept
	{
		if (!HasResolver() || resolver_.findElement == nullptr || parent == 0 || id == nullptr)
		{
			return 0;
		}
		return resolver_.findElement(resolver_.context, parent, id);
	}

	UiHandle NativeUiBackend::CloneElement(UiHandle source) noexcept
	{
		if (!HasResolver() || resolver_.cloneElement == nullptr || source == 0)
		{
			return 0;
		}
		return resolver_.cloneElement(resolver_.context, source);
	}

	UiHandle NativeUiBackend::CreateText() noexcept
	{
		if (!HasResolver() || resolver_.createText == nullptr)
		{
			return 0;
		}
		return resolver_.createText(resolver_.context);
	}

	UiHandle NativeUiBackend::CreateControl() noexcept
	{
		if (!HasResolver() || resolver_.createControl == nullptr)
		{
			return 0;
		}
		return resolver_.createControl(resolver_.context);
	}

	UiHandle NativeUiBackend::CreateShape() noexcept
	{
		if (!HasResolver() || resolver_.createShape == nullptr)
		{
			return 0;
		}
		return resolver_.createShape(resolver_.context);
	}

	bool NativeUiBackend::Attach(UiHandle child, UiHandle parent) noexcept
	{
		if (!HasResolver() || child == 0 || parent == 0)
		{
			return false;
		}
		return resolver_.attach(resolver_.context, child, parent);
	}

	bool NativeUiBackend::SetText(UiHandle element, const char *text) noexcept
	{
		if (!HasResolver() || resolver_.setText == nullptr || element == 0 || text == nullptr)
		{
			return false;
		}
		return resolver_.setText(resolver_.context, element, text);
	}

	bool NativeUiBackend::SetOptions(UiHandle element, const UiElementOptions &options) noexcept
	{
		if (!HasResolver() || resolver_.setOptions == nullptr || element == 0)
		{
			return false;
		}
		return resolver_.setOptions(resolver_.context, element, &options);
	}

	bool NativeUiBackend::SetVisible(UiHandle element, bool visible) noexcept
	{
		if (!HasResolver() || resolver_.setVisible == nullptr || element == 0)
		{
			return false;
		}
		return resolver_.setVisible(resolver_.context, element, visible);
	}

	void NativeUiBackend::Destroy(UiHandle element) noexcept
	{
		if (HasResolver() && element != 0)
		{
			resolver_.destroyRecursive(resolver_.context, element);
		}
	}

	bool NativeUiBackend::HasResolver() const noexcept
	{
		return initialized_ && resolver_.context != nullptr;
	}

} // namespace dl::engineui
