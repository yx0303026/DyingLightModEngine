#pragma once

#include "sdk/EngineProjectileAbi.h"

/* 弹丸管理器概览：安装 BulletManager 与场景帧 Hook，并向模组转发回调与记录读写。 */
class EngineProjectileManager final
{
public:
	static const DlModProjectileManagerApiV1 *GetApi() noexcept;
	static void Shutdown() noexcept;

private:
	static DlModResult MODSDK_CALL Install() noexcept;
	static uint8_t MODSDK_CALL IsInstalled() noexcept;
	static void MODSDK_CALL SetUpdateCallback(
		void *context,
		DlModProjectileUpdateCallback callback) noexcept;
	static void MODSDK_CALL SetFrameCallback(
		void *context,
		DlModFrameCallback callback) noexcept;
	static uint32_t MODSDK_CALL Classify(const void *record) noexcept;
	static DlModResult MODSDK_CALL ReadKinematics(
		const void *record,
		DlModProjectileKinematicsV1 *kinematics) noexcept;
	static DlModResult MODSDK_CALL ReadPosition(
		const void *record,
		DlModVector3V1 *position) noexcept;
	static DlModResult MODSDK_CALL ReadVelocity(
		const void *record,
		DlModVector3V1 *velocity) noexcept;
	static DlModResult MODSDK_CALL WriteVelocity(
		void *record,
		const DlModVector3V1 *velocity) noexcept;
};
