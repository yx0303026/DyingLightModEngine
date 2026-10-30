#pragma once

#include "runtime/EnginePlayerEntity.h"

class EngineHumanPlayerEntity final : public EnginePlayerEntity
{
public:
	/* 人类玩家实体：玩家实体中排除暗夜猎手模型的分支。 */
	explicit EngineHumanPlayerEntity(void *instance = nullptr) noexcept;
	using EnginePlayerEntity::ReadSnapshot;
	bool IsHumanPlayer() const noexcept;
	DlModEntityKindV1 Kind() const noexcept override;
	DlModResult ReadSnapshot(DlModHumanSnapshotV1 *snapshot) const noexcept;
	static bool IsInstance(const void *instance) noexcept;
	static void *FindLocalInstance() noexcept;
};
