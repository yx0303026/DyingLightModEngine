#include "XuiEditor.h"

#include <Windows.h>

#include <algorithm>

namespace
{
    /* 主窗口概览：创建编辑器宿主窗口、转发输入消息并驱动 DX11 与 Dear ImGui 帧循环。 */
    XuiEditor* g_editor = nullptr;

    LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (g_editor != nullptr)
        {
            g_editor->HandleMessage(message, wParam, lParam);
        }
        if (message == WM_SIZE && wParam != SIZE_MINIMIZED)
        {
            return 0;
        }
        if (message == WM_DESTROY)
        {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const wchar_t className[] = L"DyingLightUIToolWindow";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassExW(&windowClass);

    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    const int windowWidth = std::min(1440, std::max(720, screenWidth - 32));
    const int windowHeight = std::min(900, std::max(520, screenHeight - 80));
    HWND window = CreateWindowExW(
        0,
        className,
        L"Dying Light XUI 编辑器",
        WS_OVERLAPPEDWINDOW,
        16,
        16,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (window == nullptr)
    {
        return 1;
    }

    XuiEditor editor;
    g_editor = &editor;
    if (!editor.Initialize(window))
    {
        g_editor = nullptr;
        DestroyWindow(window);
        return 2;
    }
    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (message.message != WM_QUIT)
    {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        editor.Render();
    }
    editor.Shutdown();
    g_editor = nullptr;
    return 0;
}
