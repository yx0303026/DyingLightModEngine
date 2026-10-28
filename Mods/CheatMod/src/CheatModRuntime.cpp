#include "CheatModRuntime.h"

#include "EngineLogger.h"

namespace cheat_mod {

	DlModResult CheatModRuntime::Initialize(const DlModHostApiV1* hostApi) noexcept
	{
		if (initialized_ || hostApi == nullptr ||
			hostApi->structSize < sizeof(DlModHostApiV1) ||
			hostApi->abiVersion != MODSDK_ABI_VERSION || hostApi->log == nullptr) {
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}

		try {
			if (!ui_.Initialize()) {
				return DLMOD_RESULT_INVALID_STATE;
			}
		}
		catch (...) {
			return DLMOD_RESULT_INVALID_STATE;
		}

		hostContext_ = hostApi->context;
		hostLog_ = hostApi->log;
		if (hostApi->structSize >= DLMOD_HOST_API_V1_MOUSE_STATE_SIZE) {
			ui_.SetMouseStateProvider(hostApi->getUiMouseState);
		}
		initialized_ = true;
		if (!input_.Start()) {
			ui_.Shutdown();
			hostContext_ = nullptr;
			hostLog_ = nullptr;
			return DLMOD_RESULT_INVALID_STATE;
		}
		hostLog_(hostContext_, static_cast<uint32_t>(EngineLogLevel::Info), "Cheat Mod loaded successfully.");
		return DLMOD_RESULT_SUCCESS;
	}

	void CheatModRuntime::Shutdown() noexcept
	{
		if (!initialized_) {
			return;
		}

		input_.Stop();
		ui_.Shutdown();
		ui_.SetMouseStateProvider(nullptr);
		if (hostLog_ != nullptr) {
			hostLog_(hostContext_, static_cast<uint32_t>(EngineLogLevel::Info), "Cheat Mod unloaded.");
		}
		hostContext_ = nullptr;
		hostLog_ = nullptr;
		initialized_ = false;
	}

	void CheatModRuntime::RenderFrame(void* graphicsContext)
	{
		if (!initialized_) {
			return;
		}
		ApplyPendingInput();
		ui_.Render(graphicsContext);
	}

	void CheatModRuntime::HandleKeyEvent(std::uint32_t virtualKey, bool isDown) noexcept
	{
		input_.HandleKeyEvent(virtualKey, isDown);
	}

	void CheatModRuntime::HandleWindowMessage(
		std::uintptr_t window,
		std::uint32_t message,
		std::uintptr_t wParam,
		std::intptr_t lParam) noexcept
	{
		if (initialized_) {
			ui_.HandleWindowMessage(window, message, wParam, lParam);
		}
	}

	bool CheatModRuntime::IsOpen() const noexcept
	{
		return ui_.IsOpen();
	}

	void CheatModRuntime::ApplyPendingInput() noexcept
	{
		if (input_.ConsumeToggleRequest()) {
			ui_.Toggle();
		}
	}

} // namespace cheat_mod
