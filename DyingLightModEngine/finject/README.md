# Finject

Finject 是面向模组开发的类型化函数注入模块。它使用 MinHook 建立 trampoline，并以 HookHandle 管理单个注入点的卸载。

每个目标需要一个唯一的空标签类型；同一个 HookPoint<Tag, Signature> 同时只管理一个目标。函数签名按 Return(Args...) 指定，当前 thunk 使用默认 C++ 函数调用约定；x64 Windows 可直接覆盖常见本机函数，x86 目标应使用 __cdecl 函数。

使用形式如下：定义一个唯一标签，使用 HookPoint<Tag, Return(Args...)> 声明注入点，再提供与原参数完全一致并在末尾追加 CallbackAction& 的普通函数。调用 HookPoint::Install(Target::ModuleOffset(...), InjectionMode::Head, &Callback) 后，保存返回的 HookHandle 直到模组卸载。

Head 在原函数前调用回调；continueOriginal 为 false 时跳过原函数。Tail 先调用原函数，再调用回调，因此此模式下 continueOriginal 不生效。Override 默认跳过原函数，回调可将 continueOriginal 设为 true 以执行原函数。对于非 void 返回值，跳过原函数时必须设置覆盖返回值，否则 Finject 会终止进程。

类型化覆盖优先使用 CallbackAction::SetReturnValue(value)。直接设置 returnValue 时，它必须指向与函数返回类型完全相同且在回调返回后仍存活的对象；该路径不做运行时类型验证。返回引用和右值引用参数不受支持；非 void 返回值及按值传递的参数需要可复制。回调抛出的异常会被捕获：头/尾模式保留或继续执行原函数结果，覆盖模式回退到执行原函数。

HookHandle 离开作用域时会尝试移除注入，也可以调用句柄的 Reset() 或对应 HookPoint<Tag, Signature>::Uninstall()。应在模组卸载或 DLL 卸载前完成卸载；在回调自身执行期间不能同步卸载同一个注入点。Finject 不会调用 MH_Uninitialize，以免影响同进程中由引擎或其他模块创建的 MinHook。
