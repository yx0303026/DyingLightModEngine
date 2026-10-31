#pragma once

/* Finject 公共类型概览：定义注入模式、目标地址、回调动作与统一运行结果。 */

#include <any>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace finject
{
	enum class InjectionMode : std::uint8_t
	{
		Head,
		Tail,
		Override
	};

	enum class Result : std::uint8_t
	{
		Success,
		InvalidArgument,
		InvalidState,
		ModuleNotLoaded,
		AlreadyRegistered,
		HookOperationFailed
	};

	enum class TargetKind : std::uint8_t
	{
		AbsoluteAddress,
		ModuleOffset
	};

	struct Target final
	{
		TargetKind kind = TargetKind::AbsoluteAddress;
		void* address = nullptr;
		std::wstring moduleName;
		std::uintptr_t offset = 0;

		static Target At(void* address)
		{
			Target target{};
			target.kind = TargetKind::AbsoluteAddress;
			target.address = address;
			return target;
		}

		template <typename FunctionPointer>
		static Target At(FunctionPointer function)
			requires (std::is_pointer_v<FunctionPointer> &&
				std::is_function_v<std::remove_pointer_t<FunctionPointer>>)
		{
			return At(reinterpret_cast<void*>(function));
		}

		static Target ModuleOffset(std::wstring moduleName, std::uintptr_t offset)
		{
			Target target{};
			target.kind = TargetKind::ModuleOffset;
			target.moduleName = std::move(moduleName);
			target.offset = offset;
			return target;
		}
	};

	struct CallbackAction final
	{
		/* 回调动作字段概览：控制原函数是否执行，以及是否用指针对象替换最终返回值。 */
		bool continueOriginal = true;
		bool overrideReturn = false;
		void* returnValue = nullptr;
		std::any returnStorage;

		template <typename Value>
		void SetReturnValue(Value&& value)
		{
			using StoredValue = std::decay_t<Value>;
			returnStorage = std::forward<Value>(value);
			returnValue = std::any_cast<StoredValue>(&returnStorage);
			overrideReturn = true;
		}
	};
}
