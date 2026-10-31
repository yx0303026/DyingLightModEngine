# UV Recovery Hook 实现总结

## 已完成的工作

我已经为 Dying Light Mod Engine 实现了紫外线恢复（UV Recovery）钩子系统的完整框架。该系统允许模组监视并阻止猎手的 UV 恢复行为（按住紫外线护盾按键时触发）。

### 1. 新增的文件

#### 头文件
- `DyingLightModEngine/include/tracking/EngineHunterUvRecoveryRuntime.h`
  - UV Recovery 运行时类声明
  - 提供 Install/Shutdown/RegisterHooks/UnregisterHooks 接口

#### 实现文件
- `DyingLightModEngine/src/tracking/EngineHunterUvRecoveryRuntime.cpp`
  - UV Recovery 钩子的核心实现
  - 状态机状态监视逻辑（States 9/10/11）
  - 线程安全的钩子回调管理

#### 汇编桩
- `DyingLightModEngine/src/tracking/EngineHunterUvRecovery.asm`
  - x64 汇编桩，拦截状态机 +0x38 字段的写入
  - 支持阻止状态切换（返回 0 时跳过写入）

#### 文档
- `docs/reverse/UvRecovery.md`
  - 完整的使用说明和 API 文档
  - 逆向工程任务清单
  - 示例代码

### 2. 修改的文件

#### SDK ABI 定义
- `DyingLightModEngine/include/sdk/EngineEntityAbi.h`
  - 新增 `DlModHunterUvRecoveryHookDescV1` 结构体
  - 新增 `ModEngineHunterUvRecoveryStartHookFn` 回调类型
  - 新增 `ModEngineHunterUvRecoveryEndHookFn` 回调类型
  - 新增安装、状态查询、钩子注册和钩子注销函数指针
  - 四个函数指针追加在 `DlModEntityApiV1` 结构体末尾，保持旧字段偏移不变
  - 新增 `DLMOD_ENTITY_API_V1_UV_RECOVERY_SIZE` 宏用于 ABI 版本检查

#### 运行时实现
- `DyingLightModEngine/src/runtime/EngineEntityRuntime.cpp`
  - 包含 `EngineHunterUvRecoveryRuntime.h` 头文件
  - 实现 `RegisterHunterUvRecoveryHooks` 函数
  - 实现 `UnregisterHunterUvRecoveryHooks` 函数
  - 在 API 表中添加函数指针

- `DyingLightModEngine/include/runtime/EngineEntityRuntime.h`
  - 声明 `RegisterHunterUvRecoveryHooks` 静态函数
  - 声明 `UnregisterHunterUvRecoveryHooks` 静态函数

#### 构建配置
- `DyingLightModEngine/CMakeLists.txt`
  - 在 `DLMOD_ENTITY_TRACKING_ASM` 中添加 `EngineHunterUvRecovery.asm`
  - 在源文件列表中添加 `EngineHunterUvRecoveryRuntime.cpp`
  - 在头文件列表中添加 `EngineHunterUvRecoveryRuntime.h`

## 技术实现细节

### 钩子原理

UV Recovery 系统通过钩住猎手状态机（`CZombieStateMachine`）的状态字段（offset +0x38）实现：

1. **状态监视**：
   - State 9: `UvHeal_Anticipation` （准备）
   - State 10: `UvHeal` （恢复中）
   - State 11: `UvHeal_Recovery` （恢复结束）

2. **钩子触发**：
   - **开始**：即将写入状态 9 时触发，可返回 0 阻止该状态写入
   - **结束**：即将写入状态 11 时触发，并报告状态机内存中的前一状态
   - 状态 10 是游戏内部阶段，本实现不单独拦截该状态写入

3. **阻止机制**：
   - 开始钩子返回 0 时，汇编桩跳过状态写入指令
   - 游戏无法进入 UvHeal 状态，从而阻止恢复

### 线程安全

- 使用 `SRWLOCK` 保护钩子状态和恢复状态
- 支持多线程环境下的并发访问
- 异常处理：钩子崩溃时记录日志并继续执行

### ABI 兼容性

- 使用 `structSize` 字段进行版本检查
- 新函数添加在 API 表末尾，保持旧模组兼容
- 定义了 `DLMOD_ENTITY_API_V1_UV_RECOVERY_SIZE` 宏供模组检查功能可用性

## 使用示例

```cpp
// 开始钩子：返回 0 阻止恢复
uint8_t MODSDK_CALL OnUvRecoveryStart(
    void* stateMachine,
    uint32_t newState,
    void* userData)
{
    Log(EngineLogLevel::Info, "UV Recovery starting...");
    return 0;  // 阻止恢复
}

// 结束钩子
void MODSDK_CALL OnUvRecoveryEnd(
    void* stateMachine,
    uint32_t previousState,
    void* userData)
{
    Log(EngineLogLevel::Info, "UV Recovery ended");
}

// 注册
DlModHunterUvRecoveryHookDescV1 hooks{};
hooks.structSize = sizeof(hooks);
hooks.abiVersion = MODSDK_ABI_VERSION;
hooks.onStart = &OnUvRecoveryStart;
hooks.onEnd = &OnUvRecoveryEnd;
hooks.userData = nullptr;

if (api == nullptr ||
    api->structSize < DLMOD_ENTITY_API_V1_UV_RECOVERY_SIZE ||
    api->installUvRecoveryTracking == nullptr ||
    api->installUvRecoveryTracking() != DLMOD_RESULT_SUCCESS)
    return;
api->registerHunterUvRecoveryHooks(&hooks);
```

## 待完成的工作

当前样本已完成静态逆向并写入运行时，但尚未在游戏内运行验证。状态 9 的写入点为 `RVA 0xDD8F27`，
原始字节为 `C7 43 38 09 00 00 00`；状态 11 的写入点为 `RVA 0xDDDDDD`，
原始字节为 `C7 47 38 0B 00 00 00`。安装逻辑会校验 `SizeOfImage == 0x1F64000`
以及两处字节，避免把地址套用到其它游戏版本。

仍需在离线游戏环境验证开始回调、返回 0 的阻止效果以及状态 11 的结束回调。

### 推荐逆向流程

1. **在 IDA 中定位关键函数**
   ```
   地址：0x180DDDC10
   功能：驱动 UvHeal 状态，使用 f_btz_uv_heal_time
   ```

2. **查找状态切换代码**
   ```
   搜索：对 [reg+38h] 的写入，其中写入值为 9/10/11
   交叉引用：状态枚举函数 0x180DD7500
   ```

3. **验证状态机布局**
   ```
   +0x10: bool - 激活状态
   +0x38: int32 - EZombieState 枚举
   +0x40/+0x44: float - 当前状态计时/阈值
   +0x70: int32 - 防御烟雾充能
   +0xA4: float - 光免疫剩余秒数
   +0xA8: uint8 - UV 格挡标记
   ```

4. **提取签名**
   ```
   IDA → Edit → Copy → Export data
   选择写入指令前后 15-20 字节
   确保签名在 DLL 中唯一
   ```

## 与现有系统的集成

### UV Shield vs UV Recovery

| 系统 | UV Shield | UV Recovery |
|------|-----------|-------------|
| 触发 | 点击按键/捶地 | **按住**按键 |
| 监视 | 计时器（+0xA4） | 状态（+0x38） |
| 频率 | 每帧 | 状态切换 |
| 状态 | 已完成 ✓ | 已完成静态闭环，待游戏内验证 |

### 共享基础设施

- 都使用 MinHook 进行钩子注入
- 都使用 x64 汇编桩拦截写入
- 都提供线程安全的回调管理
- 都支持阻止原版行为（返回 0）

## 总结

我已经完成了 UV Recovery 钩子系统的完整框架实现，包括：

- ✅ API 定义（ABI 兼容）
- ✅ 运行时管理（线程安全）
- ✅ 汇编桩（支持阻止）
- ✅ 文档（使用说明和逆向指南）
- ✅ 已填入当前游戏版本的状态写入签名

完成逆向任务后，模组将能够：
1. 监视猎手何时开始/结束 UV 恢复
2. 通过返回 0 完全阻止 UV 恢复触发
3. 实现自定义的 UV 恢复逻辑
4. 与 UV Shield 钩子协同工作

## 下一步

1. 编译并加载模组
2. 调用 `installUvRecoveryTracking()`
3. 注册 `registerHunterUvRecoveryHooks`
4. 在离线游戏环境验证开始、阻止和结束回调

祝逆向顺利！🎮
