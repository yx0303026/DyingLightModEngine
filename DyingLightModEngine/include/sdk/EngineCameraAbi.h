#pragma once

#include "sdk/EngineAbiTypes.h"

/* 相机 ABI 概览：提供活动相机的位置、组合矩阵与世界到屏幕投影。
   所有调用都应在游戏线程执行。 */
#ifdef __cplusplus
extern "C"
{
#endif

#define DLMOD_CAMERA_MATRIX_COUNT 16u

	typedef DlModResult(MODSDK_CALL *ModEngineReadCameraPositionFn)(
		DlModVector3V1 *position);
	typedef DlModResult(MODSDK_CALL *ModEngineReadCameraMatrixFn)(
		float *matrix,
		uint32_t count);
	typedef DlModResult(MODSDK_CALL *ModEngineProjectPointFn)(
		const DlModVector3V1 *world,
		float viewportWidth,
		float viewportHeight,
		float *screenX,
		float *screenY);

	typedef struct DlModCameraApiV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		ModEngineReadCameraPositionFn readPosition;
		ModEngineReadCameraMatrixFn readMatrix;
		ModEngineProjectPointFn project;
	} DlModCameraApiV1;

#define DLMOD_CAMERA_API_V1_MIN_SIZE \
	((uint32_t)(offsetof(DlModCameraApiV1, readPosition) + sizeof(ModEngineReadCameraPositionFn)))
#define DLMOD_CAMERA_API_V1_PROJECT_SIZE \
	((uint32_t)(offsetof(DlModCameraApiV1, project) + sizeof(ModEngineProjectPointFn)))

#ifdef __cplusplus
}
#endif
