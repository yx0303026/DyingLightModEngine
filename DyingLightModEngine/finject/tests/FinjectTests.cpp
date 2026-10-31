#include "finject/finject.h"

#include <cstdio>

namespace
{
	struct HeadTag final {};
	struct TailTag final {};
	struct OverrideTag final {};
	struct VoidTag final {};

	struct TestState final
	{
		inline static int integerOriginalCalls = 0;
		inline static int voidOriginalCalls = 0;
	};

	using HeadHook = finject::HookPoint<HeadTag, int(int)>;
	using TailHook = finject::HookPoint<TailTag, int(int)>;
	using OverrideHook = finject::HookPoint<OverrideTag, int(int)>;
	using VoidHook = finject::HookPoint<VoidTag, void(int)>;

	__declspec(noinline) int IntegerTarget(int value)
	{
		++TestState::integerOriginalCalls;
		return value * 2;
	}

	__declspec(noinline) void VoidTarget(int)
	{
		++TestState::voidOriginalCalls;
	}

	void HeadCallback(int value, finject::CallbackAction& action)
	{
		if (value == 2)
		{
			action.SetReturnValue(22);
		}
		else if (value == 3)
		{
			action.continueOriginal = false;
			action.SetReturnValue(31);
		}
	}

	void TailCallback(int, finject::CallbackAction& action)
	{
		action.continueOriginal = false;
		action.SetReturnValue(42);
	}

	void OverrideCallback(int value, finject::CallbackAction& action)
	{
		action.SetReturnValue(value + 100);
	}

	void VoidCallback(int, finject::CallbackAction& action)
	{
		action.continueOriginal = false;
	}

	bool Check(bool condition, const char* message)
	{
		if (!condition)
		{
			std::fprintf(stderr, "%s\n", message);
			return false;
		}
		return true;
	}

	bool TestHead()
	{
		auto hook = HeadHook::Install(
			finject::Target::At(&IntegerTarget),
			finject::InjectionMode::Head,
			&HeadCallback);
		if (!Check(hook.GetResult() == finject::Result::Success, "head 安装失败"))
		{
			return false;
		}
		if (!Check(IntegerTarget(1) == 2, "head 未继续执行原函数"))
		{
			return false;
		}
		if (!Check(IntegerTarget(2) == 22, "head 继续原函数时未覆盖返回值"))
		{
			return false;
		}
		if (!Check(IntegerTarget(3) == 31, "head 未能跳过原函数并覆盖返回值"))
		{
			return false;
		}
		if (!Check(TestState::integerOriginalCalls == 2, "head 原函数调用次数错误"))
		{
			return false;
		}
		return Check(hook.Reset() == finject::Result::Success, "head 卸载失败");
	}

	bool TestTail()
	{
		auto hook = TailHook::Install(
			finject::Target::At(&IntegerTarget),
			finject::InjectionMode::Tail,
			&TailCallback);
		if (!Check(hook.GetResult() == finject::Result::Success, "tail 安装失败"))
		{
			return false;
		}
		if (!Check(IntegerTarget(2) == 42, "tail 未覆盖返回值"))
		{
			return false;
		}
		if (!Check(TestState::integerOriginalCalls == 3, "tail 未执行原函数"))
		{
			return false;
		}
		return Check(hook.Reset() == finject::Result::Success, "tail 卸载失败");
	}

	bool TestOverride()
	{
		auto hook = OverrideHook::Install(
			finject::Target::At(&IntegerTarget),
			finject::InjectionMode::Override,
			&OverrideCallback);
		if (!Check(hook.GetResult() == finject::Result::Success, "override 安装失败"))
		{
			return false;
		}
		if (!Check(IntegerTarget(2) == 102, "override 返回值错误"))
		{
			return false;
		}
		if (!Check(TestState::integerOriginalCalls == 3, "override 执行了原函数"))
		{
			return false;
		}
		return Check(hook.Reset() == finject::Result::Success, "override 卸载失败");
	}

	bool TestVoid()
	{
		auto hook = VoidHook::Install(
			finject::Target::At(&VoidTarget),
			finject::InjectionMode::Head,
			&VoidCallback);
		if (!Check(hook.GetResult() == finject::Result::Success, "void 安装失败"))
		{
			return false;
		}
		VoidTarget(1);
		if (!Check(TestState::voidOriginalCalls == 0, "void head 未跳过原函数"))
		{
			return false;
		}
		return Check(hook.Reset() == finject::Result::Success, "void 卸载失败");
	}
}

int main()
{
	if (!TestHead() || !TestTail() || !TestOverride() || !TestVoid())
	{
		return 1;
	}
	return 0;
}
