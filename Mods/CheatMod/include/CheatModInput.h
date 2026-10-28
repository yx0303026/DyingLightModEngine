#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

namespace cheat_mod {

	class CheatModInput final {
	public:
		CheatModInput() = default;
		~CheatModInput();
		CheatModInput(const CheatModInput&) = delete;
		CheatModInput& operator=(const CheatModInput&) = delete;

		bool Start() noexcept;
		void Stop() noexcept;
		void HandleKeyEvent(std::uint32_t virtualKey, bool isDown) noexcept;
		bool ConsumeToggleRequest() noexcept;

	private:
		static void ThreadMain(CheatModInput* input) noexcept;
		void PollF2() noexcept;

		std::atomic_bool running_ = false;
		std::atomic_bool f2Down_ = false;
		std::atomic_bool toggleRequested_ = false;
		std::thread thread_;
	};

} // namespace cheat_mod
