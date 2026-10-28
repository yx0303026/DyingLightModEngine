#include "CheatModInput.h"

#include <Windows.h>

#include <chrono>

namespace cheat_mod {

	CheatModInput::~CheatModInput()
	{
		Stop();
	}

	bool CheatModInput::Start() noexcept
	{
		if (running_.exchange(true)) {
			return true;
		}
		try {
			thread_ = std::thread(&CheatModInput::ThreadMain, this);
		}
		catch (...) {
			running_.store(false);
			return false;
		}
		return true;
	}

	void CheatModInput::Stop() noexcept
	{
		if (!running_.exchange(false)) {
			return;
		}
		if (thread_.joinable()) {
			thread_.join();
		}
		f2Down_.store(false);
		toggleRequested_.store(false);
	}

	void CheatModInput::HandleKeyEvent(std::uint32_t virtualKey, bool isDown) noexcept
	{
		if (virtualKey != VK_F2) {
			return;
		}

		const bool wasDown = f2Down_.exchange(isDown);
		if (isDown && !wasDown) {
			toggleRequested_.store(true);
		}
	}

	bool CheatModInput::ConsumeToggleRequest() noexcept
	{
		return toggleRequested_.exchange(false);
	}

	void CheatModInput::ThreadMain(CheatModInput* input) noexcept
	{
		while (input != nullptr && input->running_.load()) {
			input->PollF2();
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
		}
	}

	void CheatModInput::PollF2() noexcept
	{
		const bool isDown = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
		HandleKeyEvent(VK_F2, isDown);
	}

} // namespace cheat_mod
