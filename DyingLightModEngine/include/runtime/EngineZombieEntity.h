#pragma once

#include "runtime/EngineEntity.h"

/* 僵尸实体：继承实体基类，表示 ZombieAIVis 或其 ActorDI 视觉对象。 */
class EngineZombieEntity final : public EngineEntity
{
public:
	explicit EngineZombieEntity(void *instance = nullptr) noexcept;

	bool IsZombie() const noexcept;
	DlModEntityKindV1 Kind() const noexcept override;
	static bool IsInstance(const void *instance) noexcept;
};
