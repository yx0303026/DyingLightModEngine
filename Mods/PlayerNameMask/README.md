# Player Name Mask

这是一个本地主播模式模组。它只在客户端名称显示读取路径上生成“队友 1”、“队友 2”、“猎手 1”等名称；加入/连接提示使用中性“玩家 1”、“玩家 2”编号，因为连接提示出现时游戏尚未提供可确认的阵营对象。不修改 EOS 属性、网络复制数据、Steam 名称或其他玩家的实际账号信息。

## 当前绑定

- 游戏模块：`gamedll_x64_rwdi.dll`
- 目标函数 RVA：`0x491E00`
- 目标函数用途：本地玩家名称读取
- 连接名称入口：`engine_x64_rwdi.dll!LobbyGetClientProfileName`
- 运行时绑定：仅支持 x64，并检查 PE 镜像范围和 16 字节入口特征码

目标地址来自当前本地安装版本。游戏更新后如果镜像布局变化或入口特征码不匹配，模组会拒绝安装 Hook，并通过引擎日志报告失败。

## 构建

```powershell
cmake --preset x64-release
cmake --build out/build-vs2026/x64-release --config Release --target PlayerNameMask
```

安装后目录：

```text
Dying Light/
  Mods/
    PlayerNameMask/
      player_name_mask.dll
```

## 行为说明

编号在当前进程内按对象首次出现顺序分配；人类和猎手分别计数。模组只替换本地名称读取结果，原始名称仍由游戏和网络层保留。

IDA 已确认该读取路径用于 `T_PlayerName` 玩家条目。猎手专用玩家列表、击杀提示和结算界面是否复用此路径，尚未在游戏内逐项验证。
