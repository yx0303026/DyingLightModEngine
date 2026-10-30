# 猎手紫外线护盾（UV Block）逆向记录

## 分析目标

“紫外线护盾”在游戏代码中没有独立技能名，实际就是 BTZ 猎手状态机上的**光免疫计时**
（`ZombieDefensiveSmokeLightInvulnerabilityDuration` /
`ZombieGroundPoundLightInvulnerabilityDuration`），HUD 中以 `UVBlock` 图标显示。
本次目标是定位写入该计时的底层指令，并在引擎的 Hunter 体系中提供可读写钩子。

## 样本与工具

- 文件：`E:\SteamLibrary\steamapps\common\Dying Light\gamedll_x64_rwdi.dll`
- 文件大小：31,792,192 字节，镜像大小 `0x1F64000`
- 文件时间：2026-10-26 23:51:20
- IDA Pro 9.4，x64，preferred image base `0x180000000`，无头 `idat -A -S` 批处理 + Hex-Rays

地址均为静态 VA；运行时按 `RVA = VA - 0x180000000` 换算后加模块基址。

## 已确认的证据链

### 1. 字符串与枚举

| 证据 | 地址 | 说明 |
| --- | --- | --- |
| `UVBlock` | `0x181514988` | `HudZombieAbilities` 的能力枚举 4（0 HordeEffect、1 LightDisable、2 Toxic、3 Camo、4 UVBlock），见 `0x1805C71F0` |
| `G_UvBlock` / `ShowUVBlock` / `HideUVBlock` / `UvHeal` / `FullyDrained` | `0x1814EA4xx` | `HudHunterInfo` 的 UI 动画键 |
| `m_UVBlock` | `0x1817D43E8` | `SensorTutorial` 的教学传感器字段，**与护盾无关**，属同名干扰项 |
| `uv_block_overlay.dds`、`f_btz_zombie_post_process_uv_block_*` | `.rdata` | UV 格挡的全屏后处理资源与强度曲线 |
| `&btz_stat_uv_block_uv_heal&`、`BtzZombie_UvBlockHeal` | `.rdata` | 护盾期间的统计与治疗 |
| `ZombieDefensiveSmokeLightInvulnerabilityDuration` | `0x18170DA60` | `EZombiePlayerParams` = 734 |
| `ZombieGroundPoundLightInvulnerabilityDuration` | `0x18170D6B8` | `EZombiePlayerParams` = 735 |

两个参数由 `sub_180C58E40(params, index)` 读取（枚举到字符串函数为 `0x180C59290`）。

### 2. 状态机与字段布局

`CZombieStateMachine`（BTZ 猎手专用状态机，猎手对象 `+0xC98` 处保存指针）：

| 偏移 | 类型 | 含义 |
| --- | --- | --- |
| `+0x10` | bool | 状态机是否处于激活/本地控制 |
| `+0x38` | int32 | `EZombieState`：0 Idle、1 SpitCharging、2 Spitting、3 Spitting_Recovery、4 Spitting_Empty、5 Howling、6 GroundPound、7 DefensiveSmoke、8 DefensiveSmokeEmpty、9 UvHeal_Anticipation、10 UvHeal、11 UvHeal_Recovery、12/13 BuggyCharge |
| `+0x40` / `+0x44` | float | 当前状态计时 / 阈值 |
| **`+0x70`** | **int32** | **防御烟雾剩余充能（-1 表示无限）**，`dec [rbx+70h]` 消耗 |
| **`+0xA4`** | **float** | **光免疫剩余秒数（即“紫外线护盾”）** |
| **`+0xA8`** | **uint8** | **UV 格挡标记（HUD 是否显示 UVBlock 图标）** |
| `+0xCD` | bool | 防御烟雾已激活 |

状态枚举名由 `0x180DD7500` 输出（`UvHeal_Anticipation` / `UvHeal` / `UvHeal_Recovery` 等）。

充能读取方 `sub_180DD34B0`：`v = stateMachine[0x70]`，`v == -1` 直接返回，否则回调
`owner->vtable[2592](owner, v)` 同步充能值——即 -1 是“无限充能”哨兵。

### 2.1 充能与时长（`DW/Data0.pak: data/skills/zombie_levels.xml` / `zombie_skills.xml`）

基础值（`EZombiePlayerParams`）：

| 参数 | 枚举 | 基础值 | 说明 |
| --- | --- | --- | --- |
| `ZombieSpitDefensiveSmokeSpawnAmmoCount` | 756 | **1** | 出生态充能 |
| `ZombieSpitDefensiveSmokeMaxAmmoCount` | 761 | **1** | 充能上限 |
| `ZombieSpitDefensiveSmokeAmmoRegenTime1v1/2v1/3v1/4v1` | — | 80 / 50 / 40 / 30 s | 分人数回充时间 |
| `ZombieSpitDefensiveSmokeDurationMul` | 762 | 1 | 烟雾持续时间倍率（非光免疫） |
| `ZombieDefensiveSmokeLightInvulnerabilityDuration` | 734 | **2.5 s** | **防御烟雾的紫外线免疫时长** |
| `ZombieGroundPoundLightInvulnerabilityDuration` | 735 | **0.8 s** | **捶地的紫外线免疫时长** |

技能加成（`zombie_skills.xml`）：

| 技能 | 层 | 效果 |
| --- | --- | --- |
| `DefensiveSmokeEnabled` | 7 | `ZombieDefensiveSmokeScreenEnabled = 1`，解锁烟雾 |
| `DefensiveSmokeAmmo` | 8 | `ZombieSpitDefensiveSmokeMaxAmmoCount +1` → **上限 2 次** |
| `DefensiveSmokeDuration` | 9 | `ZombieSpitDefensiveSmokeDurationMul +1`、`ZombieDefensiveSmokeLightInvulnerabilityDuration +2.1` → **4.6 s** |

因此：**满技能时 2 次充能、每次 4.6 s 紫外线免疫；未点技能时 1 次、2.5 s；捶地固定 0.8 s。**

### 3. 写入点（钩子目标）

四处指令都写 `[rbx+0A4h]`，`rbx` 即状态机指针：

| 路径 | VA | RVA | 指令 | 行为 |
| --- | --- | --- | --- | --- |
| 主更新 `sub_180DD9120`（vtable +0x28） | `0x180DD98BD` | `0xDD98BD` | `F3 0F 11 83 A4 00 00 00` (`movss [rbx+0A4h], xmm0`) | `timer = max(timer - dt, 0)`，每帧递减 |
| 客户端更新 `sub_180DD9920`（vtable +0x30） | `0x180DD9977` | `0xDD9977` | `F3 0F 11 8B A4 00 00 00` (`movss [rbx+0A4h], xmm1`) | 同样的递减，仅在本地/客户端条件成立时执行 |
| 防御烟雾 `sub_180DD8620`（事件 36） | `0x180DD87A2` | `0xDD87A2` | `F3 0F 11 83 A4 00 00 00`，后接 `C6 83 A8 00 00 00 01` | `timer = params[734]`，并把 `+0xA8` 置 1 |
| 捶地 `sub_180DDD4C0`（状态 4→5） | `0x180DDD5F9` | `0xDDD5F9` | `F3 0F 11 83 A4 00 00 00`，后接 `40 88 B3 A8 00 00 00` | `timer = max(timer, params[735])`，`+0xA8` 置 0 |

四条签名在当前 DLL 中各自唯一（16/15 字节校验），可直接用于安装前校验。

### 4. 消费方（已确认）

- **HUD**：`HudHunterInfo::sub_180566EC0` 每帧读取状态机：
  `showUVBlock = (timer > 0 || state ∈ {9,10,11}) && flag(A8) != 0`，
  随后调用 `0x180567370` 播放 `ShowUVBlock` / `HideUVBlock`。
- **后处理**：`0x180CECFB0` 从 `playerObj + 0xC18` 指向的对象读取 `+0xA4`，映射为
  `0.75–25` 与 `1.5–40` 的强度（对应 `btz_uv_resistance` 的屏幕染色与拖影）。该对象同时
  具备 `+0x10` 字节与 `+0xA4` 浮点，字段布局与状态机一致，判定为同一状态机的另一引用路径。
- **UV 回复状态**：`0x180DDDC10` 使用 `f_btz_uv_heal_time`、`f_btz_air_uv_heal_acceleration`
  驱动 `UvHeal*` 三个状态；猎手“UV 组件”在对象 `+0x1348`，`+0x10` 为能量、
  `+0x20` 为满值标记。`0x180B618A0` 的 case 3/6/7 通过 `sub_180CE1410` 改动该能量。

### 5. 未完全闭环的部分

- 逐条比对 `[reg+0A4h]` 的浮点读取后，只在状态机自身与后处理/UI 中发现消费点；
  伤害免疫的真正分支（若与计时器分离）尚未定位到唯一函数。
  因此本实现不假设免疫逻辑位置，而是直接接管游戏自己的护盾计时值：
  把 `+0xA4` 固定为非零即等价于游戏在防御烟雾/捶地期间的“光免疫窗口”。
- `+0xA8` 只影响 HUD 显示，不影响计时器本身；PIN 模式下由引擎主动置 1 以保持 HUD 一致。

## 引擎实现

新增 `EngineHunterUvShieldRuntime`（`include/tracking`、`src/tracking`）与
`EngineHunterUvShield.asm` 四个跳板桩：

1. **Timer 桩**（`0xDD98BD`）：读出待写入值 → 按模式改写 → 返回值交给跳板写回；同时按需
   把 `+0x70` 充能固定为设定值，并在 PIN 模式下点亮 `+0xA8`。
2. **Client 桩**（`0xDD9977`）：仅应用 PIN / OFF 与充能固定，倍率只走主路径，避免同帧二次缩放。
3. **Smoke 桩**（`0xDD87A2`）：绝对时长覆盖优先，其次按倍率缩放 `params[734]`。
4. **GroundPound 桩**（`0xDDD5F9`）：绝对时长覆盖优先，其次按倍率缩放 `params[735]`。

桩保存 `rbx`、全部易失 GPR、`rsp` 对齐、`xmm1..xmm5` 与 `rflags`；`xmm0`
（客户端桩为 `xmm1`）保留回调返回值，由 MinHook 跳板重放原始 `movss` 指令写回。
栈帧 0x80/0x70 字节，满足 Win64 影子空间与 16 字节对齐要求。

充能没有独立写入钩子（`dec [rbx+70h]` 只有 3 字节，紧邻 `jnz rel8`，跳板重定位不稳），
改为在同一次每帧触发的 Timer 桩里写 `+0x70`：游戏消耗充能后，本轮 Update 结束前即被
拉回设定值，效果等价于“无限/固定充能”，且不会引入新的脆弱钩子点。

### ABI

`DlModEntityApiV1` 末尾追加（旧模组按 `structSize` 判断可用性）：

- `installUvShieldTracking()` / `isUvShieldTrackingInstalled()`
- `readHunterUvShield(DlModHunterUvShieldV1*)`
- `setHunterUvShield(mode, value)`
- `setHunterUvShieldCharges(charges)` — `-1` 关闭覆盖；`0..99` 每帧把 `+0x70` 固定为该值
- `setHunterUvShieldDuration(seconds)` — `<= 0` 关闭覆盖；否则激活时直接用该秒数替换配置值

`DlModHunterUvShieldV1` 字段：`zombieState`、`smokeCharges`、`shieldSeconds`、`shieldDuration`、
`shieldFlag`、`stateMachine`、`readTimeSeconds`，由 `validFields`
（`DLMOD_HUNTER_UV_HAS_*`）标记有效项。

### 模组用法

```cpp
DlModHunterUvShieldV1 shield{};
shield.structSize = sizeof(shield);
shield.abiVersion = MODSDK_ABI_VERSION;
api->installUvShieldTracking();                 // 也可用 installHunterTracking 顺带安装
api->setHunterUvShieldCharges(-1);              // 无限充能（-1 为游戏原生哨兵）
api->setHunterUvShieldDuration(0.0F);           // 0 = 使用游戏配置值
api->readHunterUvShield(&shield);               // shield.smokeCharges / shieldSeconds / shieldDuration
```

模式：

| 模式 | 值 | 行为 |
| --- | --- | --- |
| `DLMOD_HUNTER_UV_SHIELD_MODE_GAME` | 0 | 完全交给游戏 |
| `DLMOD_HUNTER_UV_SHIELD_MODE_PIN` | 1 | 计时不低于 `value` 秒，并点亮 `+0xA8`；等价常驻护盾 |
| `DLMOD_HUNTER_UV_SHIELD_MODE_OFF` | 2 | 计时强制归零 |
| `DLMOD_HUNTER_UV_SHIELD_MODE_SCALE` | 3 | 递减与激活时长按 `value` 倍率缩放（0–100） |

`installHunterTracking()` 会在运动追踪安装成功后顺带安装护盾钩子（失败不影响运动追踪）。
