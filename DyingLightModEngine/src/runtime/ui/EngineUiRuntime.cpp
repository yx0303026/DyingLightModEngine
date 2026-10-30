#include "runtime/ui/EngineUiRuntime.h"

namespace dl::engineui
{

	/* 生命周期概览：运行时只激活一个后端，避免两套 UI 同时接管输入。 */
	EngineUiRuntime::~EngineUiRuntime()
	{
		Shutdown();
	}

	bool EngineUiRuntime::Initialize(
		UiMode mode,
		const NativeUiResolver &nativeResolver,
		const PakUiResolver &pakResolver) noexcept
	{
		if (initialized_)
		{
			return false;
		}

		mode_ = mode;
		if (mode_ == UiMode::Native)
		{
			if (!native_.Initialize(nativeResolver))
			{
				return false;
			}
		}
		else
		{
			if (!pak_.Initialize(pakResolver))
			{
				return false;
			}
		}

		initialized_ = true;
		return true;
	}

	void EngineUiRuntime::Shutdown() noexcept
	{
		pak_.Shutdown();
		native_.Shutdown();
		initialized_ = false;
	}

	bool EngineUiRuntime::IsInitialized() const noexcept
	{
		return initialized_;
	}

	UiMode EngineUiRuntime::GetMode() const noexcept
	{
		return mode_;
	}

	NativeUiBackend &EngineUiRuntime::Native() noexcept
	{
		return native_;
	}

	PakUiBackend &EngineUiRuntime::Pak() noexcept
	{
		return pak_;
	}

} // namespace dl::engineui
