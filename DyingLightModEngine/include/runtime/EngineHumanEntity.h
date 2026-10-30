#pragma once

#include "runtime/EngineEntity.h"

class EngineHumanEntity final : public EngineEntity
{
public:
	/* 人类实体：表示 HumanAIVis 或其 ActorDI 视觉对象。 */
	explicit EngineHumanEntity(void *instance = nullptr) noexcept;
	bool IsHuman() const noexcept;
	DlModEntityKindV1 Kind() const noexcept override;
	static bool IsInstance(const void *instance) noexcept;
};
