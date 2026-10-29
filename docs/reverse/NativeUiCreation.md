# Dying Light 原生 UI 创建链路（IDA 逆向记录）

## 结论概览

本报告针对 `gamedll_x64_rwdi.dll`（IDA 数据库当前加载版本）整理原生 UI 创建路径。游戏的 UI 不是 ImGui，也不是 Win32 控件，而是 `engine_x64_rwdi.dll` 导出的 GUI/游戏接口对象体系。

最小的原生 UI 生命周期如下：

1. 通过 `ILevel::RegisterUIScreen(path, flags...)` 注册一个 UI 脚本路径。
2. 通过 `ILevel::LoadUIScreen(screenId)` 创建 `IUIScreen` 实例。
3. 将返回对象转换为目标屏幕类型（例如 `HUDScreenBase` 或具体游戏屏幕类型）。
4. 从屏幕根节点取得 `IUIElement`，通过 `FindElement(id)` 查找已有节点，或通过 GUI 对象工厂创建 `IUIText`、`IUIImage`、`IUIElementControl` 等对象。
5. 使用 `AttachToElement`/`AttachChild` 挂接到屏幕树。
6. 使用 `SetRelativePos`、`SetSize`、`SetVisible`、`SetOpacity`、`SetText`、`SetTexture` 等接口设置属性。
7. 通过 `IUIManager::ShowUIScreen(screenId, data)` 显示；隐藏时调用 `IUIScreen::Hide`、`IUIElement::HideScreen` 或 `ILevel::HideAllUIScreens`。

## IDA 关键证据

### 屏幕注册与加载

`sub_1804722B0` 是一个已确认的游戏初始化路径：

- 分配游戏菜单相关状态。
- 调用 `ILevel::LoadUITextures("data\\menu\\texturedefs\\CommonTextures.def")`。
- 调用 `IGSObject::CreateObject(...)` 创建菜单脚本对象。
- 使用 `"data\\menu\\scr\\UIGameMovieBackground.xui"` 调用 `ILevel::RegisterUIScreen(...)`。
- 使用返回的 screen id 调用 `ILevel::LoadUIScreen(...)`。
- 将结果转换为 `MenuGameMovie`。

这说明 `RegisterUIScreen` 的第一个核心参数是脚本路径，`LoadUIScreen` 返回的是屏幕对象，而不是单纯的纹理句柄。

`sub_18111F3E0` 是 HUD 屏幕加载路径：

- 从对象参数复制 HUD 脚本路径。
- 当屏幕高度小于 `577` 时，将路径尾部改为 `"_sd.xui"`，并检查文件是否存在。
- 调用 `ILevel::RegisterUIScreen(...)`。
- 注册失败时输出 `[HUDDIManager::LoadHudScreen] loading HUD script %s failed`。
- 注册成功后调用 `ILevel::LoadUIScreen(...)`。
- 将实例转换为 `HUDScreenBase`，加入 HUD 屏幕链表并执行后续初始化。

### 显示入口

`sub_1805D4F20`：

```cpp
IUIManager* manager = timeline->GetIUIManager();
manager->ShowUIScreen(screenId, nullptr);
```

因此显示屏幕的推荐顺序是“注册 -> 加载 -> ShowUIScreen”，而不是直接调用屏幕对象的 `Show`。

### UI 摄像机

`sub_1807D8CF0`：

- 从当前 `ILevel` 获取视图相机。
- 将位置、前向、上向量、FOV、近裁剪面、远裁剪面复制到 UI 相机。
- 调用 `IUIManager::SetCamera(position, up, forward, fov, near, far)`。

如果创建 3D UI 或需要与游戏相机同步，必须在 UI 管理器初始化后设置 UI camera；普通 2D 屏幕通常复用游戏已经配置好的 UI camera。

## 已确认的引擎 UI 接口

以下符号来自 `gamedll_x64_rwdi.dll` 的导入表，实际实现位于 `engine_x64_rwdi.dll`：

### `ILevel`

- `GetIUIManager`
- `RegisterUIScreen`
- `LoadUIScreen`
- `FindUIScreen`
- `GetUIScreen`
- `IsUIScreenVisible`
- `HideAllUIScreens`
- `LoadUITextures` / `LoadUITexture`
- `UnloadUITextures` / `UnloadUITexture`
- `RegisterUIPack` / `RegisterUISkin`
- `LoadUIPack` / `UnloadUIPack`
- `LoadUISkin`
- `GetVisibleUIScreens`
- `LoadUITexture`

### `IUIManager`

- `ShowUIScreen`
- `SetCamera`
- `RequestTexturePrefetch`
- `FindAllGuiObjects`
- `CallOnFrameEnd`

### `IUIScreen`

- `Show`
- `Hide`
- `GetScreenId`
- `GetScriptName`
- `SetScreenWorldMatrix`
- `SetIs3dScreen`
- `Is3dScreen`

### `IUIElement`

- `FindElement`
- `AttachToElement`
- `DetachFromParent`
- `Delete`
- `DeleteThisRecursive`
- `Show` / `Hide`
- `ShowScreen` / `HideScreen`
- `SetRelativePos` / `GetRelativePos`
- `SetPos` / `GetPos`
- `SetSize` / `GetSize`
- `SetUnscaledSize`
- `SetScale`
- `SetOpacity`
- `SetVisible`
- `SetRotation` / `SetRotationZ`
- `SetAnchor`
- `GetParent`
- `GetChildren`
- `GetChildrenCount`
- `IsText` / `IsImage` / `IsControl`

### `IUIElementControl`

- `SetText`
- `SetTextExplicit`
- `SetTexture`
- `SetTextureColor`
- `SetVisual`
- `SetEnable` / `SetDisable`
- `SetDisabled`
- `SetCanFocus`
- `ForceFocus`
- `SetFocus`
- `Select` / `Deselect`
- `ProcessMouse`
- `ProcessJoy`
- `CaptureMouse` / `ClearCaptureMouse`
- `OnClickLB` / `OnClickRB`
- `SetNewNavigationConnection`

### `IUIText`

- `SetText`
- `SetTextExplicit`
- `ClearText`
- `SetFont`
- `SetFontScale`
- `SetTextColor`
- `SetTextPosFlags`
- `SetMultiline`
- `SetBold`
- `SetItalic`
- `SetUppercase`
- `SetCharacterSpacing`
- `SetOutlineSize`
- `SetAutoSizeToText`
- `GetTextExtent`

### `IUIImage`

- `SetTexture`
- `SetNoTexture`
- `SetBackgroundMaterial`
- `SetMask`
- `PlayAVI`
- `StopAVI`
- `GetTextureSize`

### `IUIShape`

- `SetVertices`
- `SetIndices`
- `SetVertexColors`
- `SetTexture`
- `SetUseVertexColor`
- `ClearVertices`

## 不使用 PAK 的可行边界

### 可以做到的部分

- 使用已经加载的游戏 UI screen 脚本和对象树。
- 通过 `FindUIScreen`、`FindElement` 查找现有 HUD/menu 节点并修改文本、位置、颜色、透明度、可见性。
- 使用 `IUIManager::CreateObject` 创建运行时 GUI 对象。
- 使用 `IUIText` 和 `IUIShape` 创建不依赖新纹理文件的文字或纯色几何 UI。
- 复用游戏已经注册和加载的 texture/material/font。
- 通过已有 `IUIElementControl` 创建可交互按钮并接入原生输入/焦点系统。

### 不能保证的部分

- 新的 `.xui`、`.def`、skin、font、texture 或 material 文件不会凭空进入资源系统。
- `ILevel::RegisterUIScreen` 仍然需要一个可解析的脚本路径；如果文件不在游戏文件系统/已加载容器中，注册会失败。
- 因此“完全不使用 PAK，同时创建一套全新带图片皮肤的原生屏幕”不可行；可以实现的是“运行时对象树 + 已有资源复用 + 文字/形状绘制”。

## 推荐的模组实现策略

### 策略 A：复用现有屏幕，运行时挂载

1. 获取当前活动 `ILevel`。
2. 通过 `GetVisibleUIScreens` 或已知 screen id 找到 HUD 根节点。
3. `FindElement` 定位稳定的挂载点。
4. 创建 `IUIText` 或 `IUIElementControl`。
5. `AttachToElement` 挂接。
6. 每帧更新位置、文本和可见性。
7. 卸载模组时调用 `DeleteThisRecursive`。

这是最适合“不使用 PAK”的方案。

### 策略 B：注册已有脚本路径

如果游戏本身已经包含目标 `.xui`，模组只调用 `RegisterUIScreen`/`LoadUIScreen`，不增加资源文件。该方案可以保留原生布局、动画和焦点导航。

### 策略 C：纯运行时 UI

只使用 `IUIElement`、`IUIText`、`IUIShape`、`IUIElementControl` 和已加载字体/材质。该方案不依赖新增 `.xui` 或 PAK，但需要自行维护对象树和输入处理。

## ABI 封装建议

不要在业务模组中硬编码 `0x18137c108` 这类导入槽地址，也不要复制 `ILevel`/`IUIElement` 的完整对象布局。SDK 层应提供按职责拆分的稳定接口，例如：

```cpp
struct DlModNativeUiApiV1
{
    uint32_t structSize;
    uint32_t abiVersion;
    void* context;

    void* (MODSDK_CALL *getActiveLevel)(void* context);
    int32_t (MODSDK_CALL *registerScreen)(void* context, const char* scriptPath, uint32_t flags);
    void* (MODSDK_CALL *loadScreen)(void* context, int32_t screenId);
    uint8_t (MODSDK_CALL *showScreen)(void* context, int32_t screenId);
    uint8_t (MODSDK_CALL *hideScreen)(void* context, int32_t screenId);
    void* (MODSDK_CALL *findElement)(void* context, void* parent, const char* id);
    void* (MODSDK_CALL *createText)(void* context, void* owner);
    void* (MODSDK_CALL *createControl)(void* context, void* owner);
    void* (MODSDK_CALL *createShape)(void* context, void* owner);
    void (MODSDK_CALL *attach)(void* context, void* child, void* parent);
    void (MODSDK_CALL *destroyRecursive)(void* context, void* element);
};
```

上面的结构只是 ABI 方向示例，不是当前版本可直接调用的最终签名。最终实现前必须在 IDA 中继续确认：

- `ILevel`、`IUIManager`、`IUIScreen` 的 this 指针来源。
- `IObject@gui::CreateObject` 的 RTTI 参数和对象所有权。
- `IUIText`/`IUIImage`/`IUIElementControl` 的 RTTI 单例地址。
- `IUIElement::AttachToElement` 的父子对象生命周期。
- 模组卸载时游戏线程与 UI 帧回调之间的同步。

## 稳定性与版本说明

- 本报告基于当前 IDA 打开的 `gamedll_x64_rwdi.dll` x64 数据库。
- 导入表名称和 C++ 接口名比函数地址更稳定；SDK 应优先按导入符号、模块导出和特征码解析。
- UI 对象必须在游戏 UI 线程/帧回调中创建和销毁，不能从任意工作线程直接调用。
- 任何涉及新资源文件的方案都应先验证游戏文件系统是否允许 loose file；否则仍需 PAK 或现有资源。
