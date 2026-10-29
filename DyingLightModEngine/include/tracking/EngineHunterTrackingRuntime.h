#pragma once

#include "sdk/EngineAbi.h"

class EngineHunterTrackingRuntime final
{
public:
	static bool Install() noexcept;
	static bool IsInstalled() noexcept;
	static void Shutdown() noexcept;
	static void* GetCurrentControlObject() noexcept;
	static bool ReadMotion(
		void* controlObject,
		DlModHunterMotionV1& motion) noexcept;
};
