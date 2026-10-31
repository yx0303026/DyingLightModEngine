# Dying Light XUI 编辑器

`DyingLightUITool` 是一个面向 Dying Light 1 的独立桌面 XUI 布局编辑器。它读取游戏 DevTools 中的 `.xui` XML，提供节点树、画布预览、布局/文本/颜色属性编辑和 XML 保存。

## XUI 结构观察

游戏目录 `DevTools/DW/Data/Menu/Scr` 中提供了 `MenuMain_PC_workshop.xui` 和 `MenuSteamWorkshop.xui` 两个可读样本。样本的格式是 XML：

- 根节点 `XuiCanvas` 带有版本属性，例如 `version="000c"`，其 `Properties` 保存画布尺寸。
- `XuiScene` 表示屏幕，拥有 `Id`、屏幕尺寸和可选 `ClassOverride`。
- `AdvGroup`、`MyText`、`MyImage`、`AdvButton`、`UIButtonWithHints`、`UIListBox` 等节点组成嵌套控件树。
- 每个控件的 `Properties` 子节点以 XML 元素保存 `Id`、`Width`、`Height`、`Position`、`Anchor`、`Text`、`ImagePath`、`Font`、`Visual`、`NavUp/NavDown` 等字段。
- 动画另存在 `Timelines/NamedFrames/Timeline/KeyFrame` 中，节点属性和顺序由编辑器原样保留。

这些样本说明，布局本身是数据驱动的 XML 树；运行时由游戏 GUI 屏幕/元素对象系统加载，而不是 Win32 控件。仓库的 [`NativeUiCreation.md`](../docs/reverse/NativeUiCreation.md) 记录了从屏幕注册、加载到 `IUIElement` 树的原生引擎接口分析。游戏常规内容还位于 `.pak` 资源包中，本工具不解包或改写 PAK。

## 使用

从仓库根目录使用 Visual Studio 2026 的 CMake 配置生成并构建 `DyingLightUITool`。构建产物位于 `build/x64/RELEASE/DyingLightUITool.exe`。

首次启动会尝试打开游戏 DevTools 的主菜单样本；之后可用“打开”载入任意 `.xui`。左侧选择节点，中央画布拖动位置，右侧编辑属性；“添加组/文本/图片”用于创建基础节点。保存会输出可读 XML，并保留当前编辑范围之外的字段和时间线。

画布用于布局校对，不是游戏渲染器：游戏资源包中的图片、材质、字体和视觉样式不会在编辑器内真实解码或渲染，预览会以占位区域呈现。它不会修改游戏文件，也不负责把新资源打包进 PAK。

## 构建与测试

```powershell
cmake -S . -B build/cmake-ui-tool -G "Visual Studio 18 2026" -A x64
& "D:\VisualStudio2026\MSBuild\Current\Bin\MSBuild.exe" build/cmake-ui-tool/DyingLightModSuite.slnx /m /p:Configuration=Release /p:Platform=x64 /t:DyingLightUITool
ctest --test-dir build/cmake-ui-tool -C Release --output-on-failure
```
