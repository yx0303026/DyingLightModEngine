#pragma once

#include "CheatModInput.h"
#include "CheatModUi.h"

#include "EngineAbi.h"

#include <cstdint>

namespace cheat_mod {

	class CheatModRuntime final {
	public:
		CheatModRuntime() = default;
		~CheatModRuntime() = default;
		CheatModRuntime(const CheatModRuntime&) = delete;
		CheatModRuntime& operator=(const CheatModRuntime&) = delete;

		DlModResult Initialize(const DlModHostApiV1* hostApi) noexcept;
		void Shutdown() noexcept;
		void RenderFrame(void* graphicsContext);
		void HandleKeyEvent(std::uint32_t virtualKey, bool isDown) noexcept;
		void HandleWindowMessage(
			std::uintptr_t window,
			std::uint32_t message,
			std::uintptr_t wParam,
			std::intptr_t lParam) noexcept;
		bool IsOpen() const noexcept;

	private:
		void ApplyPendingInput() noexcept;

		CheatModUi ui_;
		CheatModInput input_;
		void* hostContext_ = nullptr;
		EngineLogCallback hostLog_ = nullptr;
		bool initialized_ = false;
	};

} // namespace cheat_mod
