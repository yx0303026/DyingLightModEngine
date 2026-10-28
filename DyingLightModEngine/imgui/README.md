# Engine ImGui UI

This layer provides a retained element tree over Dear ImGui. It contains core
layout panels, common controls, navigation, and tabs. It does not select or
initialize a platform or renderer backend; the host must integrate those with
the game's window and graphics device.

Available building blocks include `Grid`, `StackPanel`, `Panel`, `ScrollView`,
`ListView`, `Page`, `Button`, `Label`, `CheckBox`, `ComboBox`, `TextBox`,
`Slider`, `ProgressBar`, `Separator`, `NavigationView`, and `TabView`.

Include `dl_imgui/imgui.h` and link the CMake target
`DyingLightModEngine::ImGui`. This static target lets each mod compile the UI
and Dear ImGui implementation into its own module, avoiding a C++ class ABI
across DLL boundaries. The UI context owns an ImGui context. Call the platform and renderer backend frame
functions before `UiContext::BeginFrame`, render the root element, call
`UiContext::EndFrame`, then submit `ImGui::GetDrawData()` through the renderer
backend.

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
