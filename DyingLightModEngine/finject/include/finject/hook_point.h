#pragma once

/* Finject HookPoint 概览：为唯一标签和函数签名生成类型化入口，并管理单目标注入生命周期。 */

#include "finject/runtime.h"
#include "finject/hook_handle.h"

#include <atomic>
#include <exception>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

namespace finject
{
	template <typename Tag, typename Signature>
	class HookPoint;

	template <typename Tag, typename ReturnType, typename... Args>
	class HookPoint<Tag, ReturnType(Args...)> final
	{
	public:
		using Function = ReturnType (*)(Args...);
		using Callback = void (*)(Args..., CallbackAction&);
		static_assert(!std::is_reference_v<ReturnType>, "Finject 不支持引用返回类型");
		static_assert(
			(std::is_void_v<ReturnType> || std::is_copy_constructible_v<ReturnType>) &&
			((std::is_reference_v<Args> || std::is_copy_constructible_v<Args>) && ...),
			"Finject 要求非 void 返回值和按值参数可复制");
		static_assert((!std::is_rvalue_reference_v<Args> && ...), "Finject 不支持右值引用参数");

		static HookHandle Install(
			const Target& target,
			InjectionMode mode,
			Callback callback) noexcept
		{
			const std::lock_guard<std::mutex> lock(registrationMutex_);
			if (mode != InjectionMode::Head && mode != InjectionMode::Tail &&
				mode != InjectionMode::Override)
			{
				return HookHandle(nullptr, Result::InvalidArgument);
			}
			if (installed_.load(std::memory_order_acquire))
			{
				return HookHandle(nullptr, Result::AlreadyRegistered);
			}
			if (callback == nullptr)
			{
				return HookHandle(nullptr, Result::InvalidArgument);
			}

			void* resolvedTarget = nullptr;
			const Result resolveResult = Runtime::Resolve(target, &resolvedTarget);
			if (resolveResult != Result::Success)
			{
				return HookHandle(nullptr, resolveResult);
			}

			mode_.store(mode, std::memory_order_release);
			callback_.store(callback, std::memory_order_release);
			target_.store(resolvedTarget, std::memory_order_release);
			const Result installResult = Runtime::Install(
				resolvedTarget,
				reinterpret_cast<void*>(&Detour),
				&PublishOriginal,
				nullptr);
			if (installResult != Result::Success)
			{
				original_.store(nullptr, std::memory_order_release);
				callback_.store(nullptr, std::memory_order_release);
				target_.store(nullptr, std::memory_order_release);
				return HookHandle(nullptr, installResult);
			}

			installed_.store(true, std::memory_order_release);
			return HookHandle(&ResetThunk, Result::Success);
		}

		static Result Uninstall() noexcept
		{
			return ResetThunk();
		}

	private:
		class Invocation final
		{
		public:
			Invocation() noexcept
			{
				activeCalls_.fetch_add(1, std::memory_order_acq_rel);
				++activeOnCurrentThread_;
			}

			~Invocation()
			{
				--activeOnCurrentThread_;
				activeCalls_.fetch_sub(1, std::memory_order_acq_rel);
			}
		};

		static Result ResetThunk() noexcept
		{
			const std::lock_guard<std::mutex> lock(registrationMutex_);
			if (!installed_.load(std::memory_order_acquire))
			{
				return Result::InvalidState;
			}
			if (activeOnCurrentThread_ != 0)
			{
				return Result::InvalidState;
			}
			void* target = target_.load(std::memory_order_acquire);
			const Result removeResult = Runtime::Remove(target);
			if (removeResult != Result::Success)
			{
				return removeResult;
			}
			while (activeCalls_.load(std::memory_order_acquire) != 0)
			{
				std::this_thread::yield();
			}
			original_.store(nullptr, std::memory_order_release);
			callback_.store(nullptr, std::memory_order_release);
			target_.store(nullptr, std::memory_order_release);
			installed_.store(false, std::memory_order_release);
			return Result::Success;
		}

		static void PublishOriginal(void* trampoline, void*) noexcept
		{
			original_.store(reinterpret_cast<Function>(trampoline), std::memory_order_release);
		}

		static bool InvokeCallback(CallbackAction& action, Args&... args) noexcept
		{
			try
			{
				const Callback callback = callback_.load(std::memory_order_acquire);
				if (callback == nullptr)
				{
					return false;
				}
				callback(args..., action);
				return true;
			}
			catch (...)
			{
				return false;
			}
		}

		static ReturnType ReadReturnValue(CallbackAction& action)
			requires (!std::is_void_v<ReturnType>)
		{
			using StoredReturn = std::remove_cv_t<std::remove_reference_t<ReturnType>>;
			if (action.returnValue == nullptr)
			{
				std::terminate();
			}
			if (action.returnStorage.has_value())
			{
				StoredReturn* stored = std::any_cast<StoredReturn>(&action.returnStorage);
				if (stored == nullptr)
				{
					std::terminate();
				}
				return static_cast<ReturnType>(*stored);
			}
			return static_cast<ReturnType>(*static_cast<StoredReturn*>(action.returnValue));
		}

		static ReturnType Detour(Args... args)
		{
			Invocation invocation;
			CallbackAction action{};
			const Function original = original_.load(std::memory_order_acquire);
			if (original == nullptr)
			{
				std::terminate();
			}
			const InjectionMode mode = mode_.load(std::memory_order_acquire);
			if (mode == InjectionMode::Head)
			{
				if (!InvokeCallback(action, args...))
				{
					action = {};
				}
				if (action.continueOriginal)
				{
					if constexpr (std::is_void_v<ReturnType>)
					{
						original(args...);
						return;
					}
					else
					{
						ReturnType result = original(args...);
						return action.overrideReturn ? ReadReturnValue(action) : result;
					}
				}
				if constexpr (std::is_void_v<ReturnType>)
				{
					return;
				}
				else
				{
					return ReadReturnValue(action);
				}
			}

			if (mode == InjectionMode::Tail)
			{
				if constexpr (std::is_void_v<ReturnType>)
				{
					original(args...);
					if (!InvokeCallback(action, args...))
					{
						action.overrideReturn = false;
					}
					return;
				}
				else
				{
					ReturnType result = original(args...);
					if (!InvokeCallback(action, args...))
					{
						action.overrideReturn = false;
					}
					return action.overrideReturn ? ReadReturnValue(action) : result;
				}
			}

			action.continueOriginal = false;
			if (!InvokeCallback(action, args...))
			{
				action.continueOriginal = true;
				action.overrideReturn = false;
			}
			if (action.continueOriginal)
			{
				if constexpr (std::is_void_v<ReturnType>)
				{
					original(args...);
					return;
				}
				else
				{
					ReturnType result = original(args...);
					return action.overrideReturn ? ReadReturnValue(action) : result;
				}
			}
			if constexpr (std::is_void_v<ReturnType>)
			{
				return;
			}
			else
			{
				return ReadReturnValue(action);
			}
		}

		inline static std::mutex registrationMutex_{};
		inline static std::atomic<std::uint32_t> activeCalls_{0};
		inline static thread_local std::uint32_t activeOnCurrentThread_ = 0;
		inline static std::atomic<Function> original_{nullptr};
		inline static std::atomic<Callback> callback_{nullptr};
		inline static std::atomic<void*> target_{nullptr};
		inline static std::atomic<InjectionMode> mode_{InjectionMode::Head};
		inline static std::atomic_bool installed_{false};
	};
}
