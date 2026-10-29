#pragma once

#include "sdk/EngineAbi.h"

#include <Windows.h>

namespace custom_pak_mod {

bool StartPakLoading(HMODULE ownerModule, const DlModHostApiV1* hostApi) noexcept;
void StopPakLoading() noexcept;

}
