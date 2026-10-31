# UV Recovery Hook 使用说明

## 概述

UV Recovery（紫外线恢复）是猎手按住紫外线护盾按键时触发的恢复机制。该系统通过钩住当前游戏版本中已验证的状态写入点来监视 UvHeal 流程：

- **State 9**: `UvHeal_Anticipation` （准备阶段）
- **State 10**: `UvHeal` （恢复中，状态机内部阶段）
- **State 11**: `UvHeal_Recovery` （恢复结束写入点）

## API 使用

### 1. 安装追踪

UV Recovery 追踪需要独立安装：

```cpp
// 首先确保游戏模块已加载
const DlModEntityApiV1* api = /* 获取 API 指针 */;

// 安装 UV Recovery 追踪
if (api == nullptr ||
    api->structSize < DLMOD_ENTITY_API_V1_UV_RECOVERY_SIZE ||
    api->installUvRecoveryTracking == nullptr ||
    api->installUvRecoveryTracking() != DLMOD_RESULT_SUCCESS) {
    // 安装失败
}
```

### 2. 注册钩子

```cpp
#include "sdk/EngineEntityAbi.h"

// 开始钩子：返回 0 阻止恢复，返回 1 允许恢复
uint8_t MODSDK_CALL OnUvRecoveryStart(
    void* stateMachine,
    uint32_t newState,
    void* userData)
{
    // 示例：阻止所有 UV 恢复
    return 0;  // 返回 0 阻止状态切换
    
    // 或者允许恢复
    // return 1;
}

// 结束钩子：即将写入状态 11 时调用
void MODSDK_CALL OnUvRecoveryEnd(
    void* stateMachine,
    uint32_t previousState,
    void* userData)
{
    // 恢复结束时的清理工作
}

// 注册钩子
DlModHunterUvRecoveryHookDescV1 hooks{};
hooks.structSize = sizeof(hooks);
hooks.abiVersion = MODSDK_ABI_VERSION;
hooks.onStart = &OnUvRecoveryStart;
hooks.onEnd = &OnUvRecoveryEnd;
hooks.userData = nullptr;  // 可选的用户数据指针

api->registerHunterUvRecoveryHooks(&hooks);
```

### 3. 卸载钩子

```cpp
// 模组卸载前必须调用
api->unregisterHunterUvRecoveryHooks();
```

## 重要说明

### 当前版本的逆向结论

当前样本为 `gamedll_x64_rwdi.dll`，映像大小 `0x1F64000`。已验证的两个写入点如下：

| 用途 | RVA | 原始字节 | 基址 |
| --- | ---: | --- | --- |
| 进入 `UvHeal_Anticipation`（9） | `0xDD8F27` | `C7 43 38 09 00 00 00` | `RBX` |
| 写入 `UvHeal_Recovery`（11） | `0xDDDDDD` | `C7 47 38 0B 00 00 00` | `RDI` |

安装时会同时校验映像大小和这两处原始字节。开始桩返回 0 时跳过 MinHook 跳板中的前 7 字节，继续执行原始指令之后的代码。

### 逆向分析起点

根据文档 `docs/reverse/UvShield.md`：

- **函数地址**: `0x180DDDC10` 使用 `f_btz_uv_heal_time` 和 `f_btz_air_uv_heal_acceleration` 驱动 UvHeal 状态
- **UV 组件**: 猎手对象 `+0x1348` 处，`+0x10` 为能量，`+0x20` 为满值标记
- **状态枚举输出**: `0x180DD7500` 函数输出状态名称

推荐使用 IDA Pro + Hex-Rays 进行分析：

```
1. `0x180DDDC10` 已确认是 UvHeal 结束逻辑
2. 状态 9 写入点位于 `RVA 0xDD8F27`
3. 状态 11 写入点位于 `RVA 0xDDDDDD`
4. 运行时校验两处七字节指令后安装钩子
```

## 状态转换流程

```
正常流程：
Idle/其他状态 → UvHeal_Anticipation (9) → UvHeal (10) → UvHeal_Recovery (11) → Idle

被阻止：
其他状态 → (尝试进入 UvHeal_Anticipation) → [钩子返回 0] → 保持原状态

钩子触发时机：
- onStart: 游戏准备写入状态 9，且当前状态机尚未处于本轮恢复流程
- onEnd: 游戏准备写入状态 11，且此前已通过状态 9 进入本轮恢复流程
- 状态 10 是游戏内部恢复阶段，本版本不单独暴露状态 10 写入钩子
```

## 与 UV Shield 的区别

| 特性 | UV Shield | UV Recovery |
|------|-----------|-------------|
| 触发方式 | 点击护盾按键 / 捶地 | **按住**护盾按键 |
| 监视目标 | 状态机 `+0xA4` 计时器 | 状态机 `+0x38` 状态枚举 |
| 钩子粒度 | 计时器写入（每帧） | 状态 9 开始和状态 11 完成 |
| 阻止机制 | 返回 0 跳过计时器写入 | 返回 0 跳过状态切换 |
| 实现状态 | **已完成并工作** | **已完成静态闭环，待游戏内验证** |

## 示例：完全禁用 UV Recovery

```cpp
uint8_t MODSDK_CALL BlockAllUvRecovery(
    void* stateMachine,
    uint32_t newState,
    void* userData)
{
    // 始终阻止进入 UvHeal 状态
    return 0;
}

void SetupMod(const DlModHostApiV1* host) {
    const DlModEntityApiV1* entity = host->getEntityApi();
    
    // 安装追踪
    if (entity->installUvRecoveryTracking() != DLMOD_RESULT_SUCCESS) {
        // 记录错误
        return;
    }
    
    // 注册钩子
    DlModHunterUvRecoveryHookDescV1 hooks{};
    hooks.structSize = sizeof(hooks);
    hooks.abiVersion = MODSDK_ABI_VERSION;
    hooks.onStart = &BlockAllUvRecovery;
    hooks.onEnd = nullptr;  // 不需要结束钩子
    hooks.userData = nullptr;
    
    entity->registerHunterUvRecoveryHooks(&hooks);
}
```

## TODO List

- [x] 使用 IDA Pro 定位状态机 +0x38 的写入指令
- [x] 提取当前版本的字节签名
- [x] 更新运行时安装与阻止跳转逻辑
- [ ] 在游戏中测试验证功能
- [ ] 添加单元测试
- [x] 更新此文档移除占位签名说明

## 参考资料

- `docs/reverse/UvShield.md` - UV Shield 逆向文档
- `DyingLightModEngine/src/tracking/EngineHunterUvShieldRuntime.cpp` - UV Shield 实现参考
- `DyingLightModEngine/src/tracking/EngineHunterUvRecovery.asm` - 汇编桩实现
