#pragma once

#include "sdk/EngineAbiTypes.h"

/* 碰撞 ABI 概览：定义命中箱、线段与调用方提供存储区的快照接口。 */
#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct DlModHitboxV1
	{
		uint32_t structSize;
		uint32_t elementIndex;
		uint32_t flags;
		DlModVector3V1 localMinimum;
		DlModVector3V1 localMaximum;
		/* 世界变换为行优先 3x4 矩阵，包含三个基向量行和一个平移行。 */
		float worldMatrix[12];
		char name[128];
		DlModVector3V1 worldCenter;
	} DlModHitboxV1;

	typedef struct DlModCollisionLineV1
	{
		DlModVector3V1 start;
		DlModVector3V1 end;
	} DlModCollisionLineV1;

	typedef struct DlModHitboxSnapshotV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t traceMode;
		uint32_t unsupportedCount;
		uint32_t requiredCount;
		uint32_t count;
		uint32_t capacity;
		uint32_t elementSize;
		DlModHitboxV1 *elements;
		/* 可选扩展输出；只需要命中箱部件时可以省略。 */
		uint32_t usesBoxQueries;
		uint32_t wireframeRequiredCount;
		uint32_t wireframeCount;
		uint32_t wireframeCapacity;
		uint32_t shapeCount;
		uint32_t wireframeTruncated;
		DlModCollisionLineV1 *wireframeLines;
	} DlModHitboxSnapshotV1;

#define DLMOD_HITBOX_V1_BASE_SIZE \
	((uint32_t)offsetof(DlModHitboxV1, worldCenter))
#define DLMOD_HITBOX_SNAPSHOT_V1_BASE_SIZE \
	((uint32_t)offsetof(DlModHitboxSnapshotV1, usesBoxQueries))

	/* 采集必须在游戏线程执行；elements 指向调用方拥有的存储区。 */
	typedef DlModResult(MODSDK_CALL *ModEngineCaptureHitboxesFn)(
		void *controlObject,
		DlModHitboxSnapshotV1 *snapshot);
	typedef DlModResult(MODSDK_CALL *ModEngineGetHitboxByNameFn)(
		void *controlObject,
		const char *name,
		DlModHitboxV1 *hitbox,
		uint32_t hitboxSize);

	typedef struct DlModCollisionApiV1
	{
		uint32_t structSize;
		uint32_t abiVersion;
		ModEngineCaptureHitboxesFn captureHitboxes;
		ModEngineGetHitboxByNameFn getHitboxByName;
	} DlModCollisionApiV1;

#define DLMOD_COLLISION_API_V1_MIN_SIZE \
	((uint32_t)(offsetof(DlModCollisionApiV1, captureHitboxes) + sizeof(ModEngineCaptureHitboxesFn)))
#define DLMOD_COLLISION_API_V1_HITBOX_NAME_SIZE \
	((uint32_t)(offsetof(DlModCollisionApiV1, getHitboxByName) + sizeof(ModEngineGetHitboxByNameFn)))

#ifdef __cplusplus
}
#endif
