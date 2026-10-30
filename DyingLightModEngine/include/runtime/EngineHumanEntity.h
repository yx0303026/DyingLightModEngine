#pragma once

#include "runtime/EngineEntity.h"

/* 人类实体：继承实体基类，表示非玩家的人类视觉实体。 */
class EngineHumanEntity final : public EngineEntity
{
public:
	explicit EngineHumanEntity(void *instance = nullptr) noexcept;

	bool IsHuman() const noexcept;
	DlModEntityKindV1 Kind() const noexcept override;
	static bool IsInstance(const void *instance) noexcept;
};
