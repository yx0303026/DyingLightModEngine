# 唾液碰撞验证 CT

## 文件

- `DyingLightSpitValidation.ct`：只读校验模块和唾液 Raytrace、命中回调地址。
- `DyingLightSpitNoStickExperiment.ct`：只在 `StickToSurfaces` 参数解析处强制写入 false，保留命中回调、伤害和特效链。

## 使用顺序

1. 在单人模式启动游戏，并让 Cheat Engine 附加 `DyingLightGame.exe`。
2. 加载验证 CT，启用验证项并确认地址校验通过。
3. 关闭验证项，再启用禁止粘附实验项。必须在进入关卡或生成唾液前启用，使新加载的投掷物参数使用 false。
4. 分别测试唾液命中墙面和实体，确认是否仍造成伤害、播放命中特效。
5. 测试结束后关闭实验项，脚本会恢复原始参数解析调用。

## 当前版本证据

- 唾液 Raytrace：`gamedll_x64_rwdi.dll+0xEEBD30`，静态函数 `sub_180EEBD30`。
- 碰撞结果回调观察点：`gamedll_x64_rwdi.dll+0xEEC176`，原始指令 `FF 90 00 04 00 00`。该点不作为最终补丁。
- `StickToSurfaces` 布尔转换调用点：`gamedll_x64_rwdi.dll+0x73C1A4`，原始字节 `E8 A7 02 BA 00`，返回值随后写入参数对象 `+0x35C`。前一个 `+0x73C19C` 是取参数值的调用，不应修改。
- `CasterSpit` 专属类大小约 `0x5D0`。

## 风险说明

实验项只改写 `StickToSurfaces` 的布尔返回值，仍建议只用于离线单人验证，不用于在线对战。若唾液仍然粘附，说明当前资源可能在别处直接设置了粘附标志，需要继续追踪 `+0x35C` 的写入和物理绑定调用。
