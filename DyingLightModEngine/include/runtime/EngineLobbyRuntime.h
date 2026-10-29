#pragma once

#include "sdk/EngineLobbyAbi.h"

// 对模组提供大厅快照 ABI，并管理 Engine 内部大厅采集器生命周期。
class EngineLobbyRuntime final
{
public:
	static bool Initialize() noexcept;
	static void Shutdown() noexcept;
	static const DlModLobbyApiV1* GetApi() noexcept;
	static void* GetControlObject() noexcept;

private:
	static DlModResult MODSDK_CALL Capture(
		void* controlObject,
		DlModLobbySnapshotV1* snapshot) noexcept;
	static DlModResult MODSDK_CALL ClearHistory(void* controlObject) noexcept;
};

