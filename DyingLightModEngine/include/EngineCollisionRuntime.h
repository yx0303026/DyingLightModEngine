#pragma once

#include "EngineAbi.h"

class EngineCollisionRuntime final {
public:
	static const DlModCollisionApiV1* GetApi() noexcept;

private:
	static DlModResult MODSDK_CALL CaptureHitboxes(
		void* controlObject,
		DlModHitboxSnapshotV1* snapshot) noexcept;
	static DlModResult MODSDK_CALL GetHitboxByName(
		void* controlObject,
		const char* name,
		DlModHitboxV1* hitbox,
		uint32_t hitboxSize) noexcept;
};
