#pragma once

#include "runtime/EngineEntity.h"

class EnginePlayerEntity : public EngineEntity
{
public:
	/* 玩家实体：继承实体基类并约束为 PlayerDI。 */
	explicit EnginePlayerEntity(void *instance = nullptr) noexcept;
	using EngineEntity::ReadSnapshot;
	bool IsPlayer() const noexcept;
	DlModEntityKindV1 Kind() const noexcept override;
	DlModResult ReadSnapshot(DlModPlayerSnapshotV1 *snapshot) const noexcept;
	static bool IsInstance(const void *instance) noexcept;
	static void *FindLocalInstance() noexcept;
};
