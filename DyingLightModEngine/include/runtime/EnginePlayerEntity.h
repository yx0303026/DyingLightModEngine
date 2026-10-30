#pragma once

#include "runtime/EngineEntity.h"

/* 玩家实体：在实体基类之上提供 PlayerDI 类型校验和玩家快照。 */
class EnginePlayerEntity : public EngineEntity
{
public:
	explicit EnginePlayerEntity(void *instance = nullptr) noexcept;
	using EngineEntity::ReadSnapshot;

	bool IsPlayer() const noexcept;
	DlModEntityKindV1 Kind() const noexcept override;
	DlModResult ReadSnapshot(
		DlModPlayerSnapshotV1 *snapshot) const noexcept;

	static bool IsInstance(const void *instance) noexcept;
	static void *FindLocalInstance() noexcept;
};
