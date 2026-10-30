#pragma once

#include "runtime/EnginePlayerEntity.h"

/* 人类玩家实体：继承玩家实体，表示非暗夜猎手的玩家角色。 */
class EngineHumanPlayerEntity final : public EnginePlayerEntity
{
public:
	explicit EngineHumanPlayerEntity(void *instance = nullptr) noexcept;
	using EnginePlayerEntity::ReadSnapshot;

	bool IsHumanPlayer() const noexcept;
	DlModEntityKindV1 Kind() const noexcept override;
	DlModResult ReadSnapshot(
		DlModHumanSnapshotV1 *snapshot) const noexcept;

	static bool IsInstance(const void *instance) noexcept;
	static void *FindLocalInstance() noexcept;
};
