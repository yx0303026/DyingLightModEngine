#pragma once

/* Finject 生命周期句柄概览：提供可移动的 RAII 句柄并在释放时卸载对应 HookPoint。 */

#include "finject/types.h"

namespace finject
{
	class HookHandle final
	{
	public:
		HookHandle() noexcept = default;
		HookHandle(const HookHandle&) = delete;
		HookHandle& operator=(const HookHandle&) = delete;
		HookHandle(HookHandle&& other) noexcept;
		HookHandle& operator=(HookHandle&& other) noexcept;
		~HookHandle();

		bool IsActive() const noexcept;
		Result GetResult() const noexcept;
		Result Reset() noexcept;

	private:
		using ResetFunction = Result (*)() noexcept;

		HookHandle(ResetFunction reset, Result result) noexcept;

		ResetFunction reset_ = nullptr;
		Result result_ = Result::InvalidState;

		template <typename Tag, typename Signature>
		friend class HookPoint;
	};
}
