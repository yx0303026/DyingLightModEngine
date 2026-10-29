#pragma once

#include "EngineAbi.h"

class EngineCollisionRuntime final {
public:
	static const DlModCollisionApiV1* GetApi() noexcept;

private:
	static DlModResult MODSDK_CALL CaptureHitboxes(
		void* controlObject,
		DlModHitboxSnapshotV1* snapshot) noexcept;
};
