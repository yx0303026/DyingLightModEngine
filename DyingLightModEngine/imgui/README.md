# Engine ImGui UI

This layer provides a retained element tree over Dear ImGui, common controls,
navigation, and frame hooks for DX11, DX12, OpenGL, Vulkan, and Vuk. Hooks only
provide a render callback at the graphics presentation boundary. The host still
owns device/window discovery and initialization of the matching ImGui platform
and renderer backends.

Available building blocks include `Grid`, `StackPanel`, `Panel`, `ScrollView`,
`ListView`, `Page`, `Button`, `Label`, `CheckBox`, `ComboBox`, `TextBox`,
`Slider`, `ProgressBar`, `Separator`, `NavigationView`, and `TabView`.

Include `dl_imgui/imgui.h` and link the CMake target
`DyingLightModEngine::ImGui`. This static target lets each mod compile the UI
and Dear ImGui implementation into its own module, avoiding a C++ class ABI
across DLL boundaries. The UI context owns an ImGui context. The backend frame
functions must run before `UiContext::BeginFrame`; after rendering, the host
submits `ImGui::GetDrawData()` through the matching renderer backend.

## Render Hooks

Include `dl_imgui/hooks.h`. DX11 and DX12 hooks install against a live swap
chain and intercept its `Present` method. OpenGL hooks intercept `gdi32!SwapBuffers`.
Vulkan hooks require the `vkQueuePresentKHR` function pointer returned by the
active instance/device dispatch; the Vuk adapter accepts the `vkQueuePresentKHR`
dispatch function used by Vuk. Each hook invokes its callback before forwarding to the original
presentation function. Install hooks only after the graphics objects exist and
uninstall them before those objects or the module are destroyed.

```cpp
class Overlay final {
public:
    void RenderFrame();

private:
    dl::imgui::UiContext context;
    dl::imgui::NavigationView root{180.0F};
};

void Overlay::RenderFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    context.BeginFrame();
    context.Render(root);
    context.EndFrame();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

Overlay overlay;
dl::imgui::Dx11Hook hook;
hook.Install(swapChain, std::bind(&Overlay::RenderFrame, &overlay));
```

The hook implementation uses MinHook internally. Callbacks execute on the
rendering thread and must not block or throw. The engine runtime creates a
small hidden DX11 swap chain to obtain the `Present` entry point, then uses the
same hook for the game's live swap chain.

User components can derive from `Component` and compose children with
`AddChild`, or derive from `Panel` to provide custom rendering while retaining
child ownership. `Grid` uses zero-based row and column indexes. `Visibility`
supports visible, hidden (keeps explicit size and margins without painting),
and collapsed elements.

`Style` can override common ImGui colors, frame rounding, and item spacing on
any element. Styles inherit through ImGui's style stack and can be overridden
by child elements.

```cpp
class SettingsPanel final : public dl::imgui::Component {
public:
    SettingsPanel()
    {
        AddChild<dl::imgui::Label>("Gameplay");
        AddChild<dl::imgui::CheckBox>("Enable feature", true);
        AddChild<dl::imgui::Slider>("Field of view", 70.0F, 50.0F, 120.0F);
    }
};

dl::imgui::UiContext context;
context.Create();

dl::imgui::NavigationView root(180.0F);
auto& settings = root.AddPage("settings", "Settings");
settings.AddChild<SettingsPanel>();
settings.AddChild<dl::imgui::ComboBox>("Resolution");

ImGui_ImplWin32_NewFrame();
ImGui_ImplDX9_NewFrame();
context.BeginFrame();
ImGui::Begin("Mod Settings");
context.Render(root);
ImGui::End();
context.EndFrame();
ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
```
