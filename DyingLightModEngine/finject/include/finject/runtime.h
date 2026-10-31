#pragma once

/* Finject 运行时接口概览：封装目标地址解析以及 MinHook 的安装和移除操作。 */

#include "finject/types.h"

namespace finject
{
	class Runtime final
	{
	public:
		using OriginalPublisher = void (*)(void* trampoline, void* context) noexcept;

		static Result Resolve(const Target& target, void** address) noexcept;
		static Result Install(
			void* target,
			void* detour,
			OriginalPublisher publishOriginal,
			void* context) noexcept;
		static Result Remove(void* target) noexcept;
	};
}
