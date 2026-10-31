#include "finject/hook_handle.h"

#include <utility>

finject::HookHandle::HookHandle(ResetFunction reset, Result result) noexcept
	: reset_(reset), result_(result)
{
}

finject::HookHandle::HookHandle(HookHandle&& other) noexcept
	: reset_(std::exchange(other.reset_, nullptr)),
	  result_(other.result_)
{
}

finject::HookHandle& finject::HookHandle::operator=(HookHandle&& other) noexcept
{
	if (this != &other)
	{
		if (Reset() != Result::Success && reset_ != nullptr)
		{
			return *this;
		}
		reset_ = std::exchange(other.reset_, nullptr);
		result_ = other.result_;
	}
	return *this;
}

finject::HookHandle::~HookHandle()
{
	Reset();
}

bool finject::HookHandle::IsActive() const noexcept
{
	return reset_ != nullptr;
}

finject::Result finject::HookHandle::GetResult() const noexcept
{
	return result_;
}

finject::Result finject::HookHandle::Reset() noexcept
{
	if (reset_ == nullptr)
	{
		return result_;
	}
	const Result resetResult = reset_();
	result_ = resetResult;
	if (resetResult == Result::Success)
	{
		reset_ = nullptr;
	}
	return resetResult;
}
