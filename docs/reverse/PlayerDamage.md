# 玩家受伤（HealthModule::AddDamageToBody）逆向记录

## 分析目标

定位"实体受到的伤害最终写入生命值"的那一次调用，并在其**入口**提供可改写伤害、
可拦截结算的前置钩子，以及结算完成后的后置钩子。

结论：真正的伤害结算点是 `CHealthModule::AddDamageToBody`（源码路径
`...\game\ai\modules\healthmodule.cpp`），它对玩家、AI、载具等所有挂了 HealthModule
的实体通用。玩家对象使用的就是同一个 HealthModule（或其 Armor/BodyParts 派生）。

## 样本与工具

- 文件：`E:\SteamLibrary\steamapps\common\Dying Light\gamedll_x64_rwdi.dll`
- 镜像大小 `0x1F64000`，preferred image base `0x180000000`
- IDA Pro 9.4，x64，Hex-Rays
- 地址均为静态 VA；运行时 `RVA = VA - 0x180000000`，再加模块基址。

## 证据链

### 1. 调试字符串（唯一确定函数身份）

| 字符串 | 地址 |
| --- | --- |
| `[HealthModule::AddDamageToBody] current health: ` | `0x181454768` |
| `[HealthModule::AddDamageToBody] remaining HealthBody: ` | `0x1814547F0` |
| `d:\perforce\...\game\ai\modules\healthmodule.cpp` | `0x181454830` |

所有引用都落在 `sub_1802A2830`（0x2C93 字节）内部，即该函数就是
`CHealthModule::AddDamageToBody`。

### 2. 类层次与虚表

| 类 | 构造函数 | 主虚表（`??_7...@@6B@`） |
| --- | --- | --- |
| `HealthModule` | `0x1802A21E0` | `0x181455158`（RVA `0x1455158`） |
| `ArmorHealthModule` | `0x1802796C0` 附近 | `0x1814451E0` |
| `BodyPartsHealthModule` | `0x180284EA0` | `0x181447AF0`（RVA `0x1447AF0`） |

`HealthModule` 由 `AIModule` 派生（构造函数先调 `sub_180272E20`），`BodyPartsHealthModule`
构造函数显式调用 `HealthModule` 构造函数，说明二者是同一族模块。

三个主虚表在 **slot 263（`+0x838`）** 都存放 `AddDamageToBody`：

| 虚表 | `+0x838` 处 | 目标 |
| --- | --- | --- |
| `HealthModule` | `0x181455990` | `0x1802A2830`（RVA `0x2A2830`） |
| `ArmorHealthModule` | `0x181445A18` | `0x180279AF0`（RVA `0x279AF0`） |
| `BodyPartsHealthModule` | `0x181448328` | `0x180279AF0` |

`0x279AF0` 是"护甲/部位"覆写，内部**再次调用**基类 `0x2A2830`：

- 第一次以 `a11 = 1` 调用（只算最终伤害，不扣主生命），结果用于部位/护甲扣血；
- 第二次以调用方传入的 `a11`（通常 0）调用，真正扣主生命。

因此一次受伤事件在基类里可能出现 **1~2 次**调用，钩子必须按"调用深度"去重。

### 3. 函数签名

`sub_1802A2830`（MSVC x64，第 3 个参数是浮点，走 `xmm2`）：

```c
float __fastcall AddDamageToBody(
    CHealthModule *self,   // rcx
    void          *source, // rdx，造成伤害的实体（可为 0）
    float          damage, // xmm2
    void          *arg4,   // r9
    unsigned       damageType, // [rsp+28h] 第 5 个参数
    int            arg6,       // [rsp+30h]
    int            arg7,       // [rsp+38h]
    int            arg8,       // [rsp+40h]
    char           arg9,       // [rsp+48h]
    char           arg10,      // [rsp+50h]
    char           skipHealth, // [rsp+58h] 非 0 时不扣主生命
    int           *arg12);     // [rsp+60h]
```

`sub_180279AF0`（Armor/BodyParts 覆写）参数寄存器布局完全一致，序言中的
`movss [rax+18h], xmm2` 同样证明伤害在 `xmm2`。

返回值：`xmm0` 为**本次结算后的最终伤害值**（`return (__m128)v21;`），
调用方用它扣部位血量（`v31 - ret * 0.5`）。

### 4. 字段布局（CHealthModule）

| 偏移 | 类型 | 含义 |
| --- | --- | --- |
| `+0x40` | void\* | 宿主实体（`AIModule` 基类字段），与 `IActor/IGameObject` 指针一致 |
| `+0x78` | float | 主生命值（`fmaxf(old - damage, 0)` 后回写，见 `sub_1800CBA40`） |
| `+0x7C` | float | 第二层血量/护甲（被 25% 上限的溢出伤害削减） |
| `+0xB0` | uint64 | 最后伤害者（`sub_18008E2D0(self+144, ...)` 记录） |

伤害结算尾段（`0x1802A4992`）：

```c
v352 = fmaxf(*(float *)(self + 120) - finalDamage, 0.0f);  // 120 == 0x78
sub_1800CBA40(self, &v352);                                // 写回 +0x78
```

`sub_1800CBA40` 只有一处 `*(float*)(a1 + 120) = *a2;`，确认偏移。

### 5. 最终伤害是怎么算出来的

`0x1802A2A4A` 附近：

```c
finalDamage = fmaxf((armorScalar + 1.0f) * damage, 1.0f) * (mulA * mulB * mulC);
```

即 **入参 `damage` 是"收到伤害"**，之后才乘上护甲、抗性、难度、部位等系数。
所以钩子改写入口 `xmm2` 等价于"改写即将到来的伤害"，原版代码仍按自己的规则继续处理。

## 引擎实现

新增 `EngineDamageRuntime`（`include/runtime/entity/damage`、`src/runtime/entity/damage`）
与 `src/runtime/entity/damage/EngineDamage.asm` 两个跳板桩：

| 桩 | 目标 | RVA |
| --- | --- | --- |
| `EngineEntityDamageOuterStub` | `ArmorHealthModule`/`BodyPartsHealthModule::AddDamageToBody` | `0x279AF0` |
| `EngineEntityDamageBaseStub` | `HealthModule::AddDamageToBody` | `0x2A2830` |

安装前用 16 字节序言签名校验，版本不符则拒绝挂接。

### 跳板流程

1. 保存 `rcx/rdx/r8/r9` 与 `xmm0-xmm3`，读取 `xmm2`（伤害位）与第 5 个参数（伤害类型）。
2. 调用 `EngineEntity_OnDamageEntry`，返回 `(允许结算 << 32) | 改写后的伤害位`。
3. **阻止**：`xorps xmm0, xmm0` 直接返回 0，原版函数完全不执行（不扣血、不触发受击）。
4. **允许**：把改写后的伤害写回 `xmm2`，复制原栈参数后以**函数方式调用跳板**
   （原版序言 + 跳回函数体），拿到 `xmm0` 返回值。
5. 调用 `EngineEntity_OnDamageReturn` 触发后置回调，随后把返回值原样送回调用方。

### 去重：线程局部调用深度

`Armor/BodyParts` 覆写内部会转调基类，两个桩都装在同一线程上，用 `t_callDepth`
区分：

- 深度 0 → 外层事件：触发**受伤前**回调，允许/阻止都在这里决定；
- 深度 > 0 → 嵌套转调：只透传，不再回调；
- 最外层返回 → 触发**受伤后**回调。

嵌套转调里 `skipHealth == 0` 的那次返回值会被记录为 `appliedDamage`。

### 对外 ABI

```c
typedef struct DlModDamageEventV1 {
    uint32_t structSize, abiVersion, flags, damageType;
    void *healthModule;   // CHealthModule
    void *entity;         // 宿主实体（DlModEntityV1.instance 可直接使用）
    void *instigator;     // 造成伤害的实体，可为 0
    float incomingDamage; // 原始伤害（只读参考）
    float damage;         // 可改写，交给原版继续处理
    float appliedDamage;  // 受伤后：游戏实际结算值
    float healthBefore, healthAfter;
    uint64_t sequence;
} DlModDamageEventV1;

uint8_t(*pre)(DlModDamageEventV1 *event);        // 返回 0 阻止本次受伤
void   (*post)(const DlModDamageEventV1 *event); // 结算结束后回调
```

| flags | 含义 |
| --- | --- |
| `DLMOD_DAMAGE_FLAG_PRE` | 受伤前阶段 |
| `DLMOD_DAMAGE_FLAG_POST` | 受伤后阶段 |
| `DLMOD_DAMAGE_FLAG_BLOCKED` | 本次受伤被前置钩子阻止 |
| `DLMOD_DAMAGE_FLAG_VICTIM_PLAYER` | 宿主实体是玩家（`PlayerDI`） |
| `DLMOD_DAMAGE_FLAG_GAME_APPLIED` | 原版结算已执行（仅在未被阻止的受伤后阶段出现） |

函数表追加在 `DlModEntityApiV1` 末尾（用 `DLMOD_ENTITY_API_V1_DAMAGE_SIZE` 探测）：

```c
DlModResult installDamageTracking(void);
uint8_t     isDamageTrackingInstalled(void);
DlModResult registerDamageHook(pre, post);
DlModResult unregisterDamageHook(pre, post);
```

### 模组用法

```c
static uint8_t MODSDK_CALL OnDamagePre(DlModDamageEventV1 *event)
{
    if ((event->flags & DLMOD_DAMAGE_FLAG_VICTIM_PLAYER) == 0)
        return 1;                 /* 只处理玩家 */
    if (event->incomingDamage > 40.0f)
        event->damage = 40.0f;    /* 改写：原版继续用 40 结算 */
    return 1;                     /* 返回 0 则本次受伤完全取消 */
}

static void MODSDK_CALL OnDamagePost(const DlModDamageEventV1 *event)
{
    /* event->appliedDamage 为游戏最终结算值；
       event->healthBefore - event->healthAfter 为实际扣血 */
}

/* 加载时 */
api->entity->installDamageTracking();
api->entity->registerDamageHook(&OnDamagePre, &OnDamagePost);

/* 卸载时 */
api->entity->unregisterDamageHook(&OnDamagePre, &OnDamagePost);
```

`entity` 与 `instigator` 都是实体基址，可直接填入 `DlModEntityV1` 复用
`readEntity` / `readPlayer` 等接口。

## 未闭环的点

- `arg4`、`arg6`~`arg10`、`arg12` 的具体语义尚未逐一验证；引擎只把第 5 个参数
  作为 `damageType` 透传，其它参数原样保留。
- `damageType` 在 `HealthModule` 与 `Armor/BodyParts` 两条路径上的取值范围不同，
  模组不应假设固定枚举。
- 若玩家实际使用的是纯 `HealthModule`（无护甲/部位覆写），钩子只在基类桩上触发一次
  （`appliedDamage` 直接取返回值），行为一致。
