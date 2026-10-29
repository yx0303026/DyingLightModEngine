#pragma once

#include "sdk/EngineCameraAbi.h"

/* 活动相机运行时概览：向模组暴露相机位置、组合矩阵与世界到视口投影。 */
class EngineCameraRuntime final
{
public:
	static const DlModCameraApiV1 *GetApi() noexcept;

private:
	static DlModResult MODSDK_CALL ReadPosition(DlModVector3V1 *position) noexcept;
	static DlModResult MODSDK_CALL ReadMatrix(float *matrix, uint32_t count) noexcept;
	static DlModResult MODSDK_CALL Project(
		const DlModVector3V1 *world,
		float viewportWidth,
		float viewportHeight,
		float *screenX,
		float *screenY) noexcept;
};
