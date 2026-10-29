#pragma once

#include "sdk/EngineAbi.h"

class EngineEntityRuntime final
{
public:
	static const DlModEntityApiV1 *GetApi() noexcept;
	static void Shutdown() noexcept;

private:
	static DlModResult MODSDK_CALL GetLocalEntity(DlModEntityV1 *entity) noexcept;
	static DlModResult MODSDK_CALL GetLocalPlayer(DlModPlayerV1 *player) noexcept;
	static DlModResult MODSDK_CALL GetLocalHuman(DlModHumanV1 *human) noexcept;
	static DlModResult MODSDK_CALL GetCurrentHunter(DlModHunterV1 *hunter) noexcept;
	static DlModResult MODSDK_CALL ReadEntity(
		const DlModEntityV1 *entity,
		DlModEntitySnapshotV1 *snapshot) noexcept;
	static DlModResult MODSDK_CALL ReadPlayer(
		const DlModPlayerV1 *player,
		DlModPlayerSnapshotV1 *snapshot) noexcept;
	static DlModResult MODSDK_CALL ReadHuman(
		const DlModHumanV1 *human,
		DlModHumanSnapshotV1 *snapshot) noexcept;
	static DlModResult MODSDK_CALL ReadHunter(
		const DlModHunterV1 *hunter,
		DlModHunterSnapshotV1 *snapshot) noexcept;
	static DlModResult MODSDK_CALL SetPosition(
		const DlModEntityV1 *entity,
		const DlModVector3V1 *position) noexcept;
	static DlModResult MODSDK_CALL SetOrientation(
		const DlModEntityV1 *entity,
		const DlModVector3V1 *forward,
		const DlModVector3V1 *up) noexcept;
	static DlModResult MODSDK_CALL ReadBonePosition(
		const DlModHunterV1 *hunter,
		uint8_t boneId,
		DlModVector3V1 *position) noexcept;
	static DlModResult MODSDK_CALL ReadHunterMotion(
		const DlModHunterV1 *hunter,
		DlModHunterMotionV1 *motion) noexcept;
	static DlModResult MODSDK_CALL InstallHunterTracking() noexcept;
	static uint8_t MODSDK_CALL IsHunterTrackingInstalled() noexcept;
};
