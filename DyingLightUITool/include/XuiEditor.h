#pragma once

#include "XuiModel.h"
#include "XuiRenderer.h"

#include <d3d11.h>
#include <dxgi.h>

#include <cstdint>
#include <string>

/* 编辑器主控制器概览：协调 Win32 窗口、Dear ImGui、文档状态、树视图、属性编辑和文件对话框。 */
class XuiEditor final
{
public:
    XuiEditor();
    ~XuiEditor();
    XuiEditor(const XuiEditor&) = delete;
    XuiEditor& operator=(const XuiEditor&) = delete;

    bool Initialize(HWND window);
    void Shutdown() noexcept;
    void Render();
    void HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) noexcept;

private:
    XuiDocument document_;
    XuiRenderer renderer_;
    XuiXmlNode* selectedNode_ = nullptr;
    HWND window_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* deviceContext_ = nullptr;
    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11RenderTargetView* renderTarget_ = nullptr;
    std::string statusText_;
    std::string lastError_;
    bool initialized_ = false;
    bool showGrid_ = true;

    bool InitializeGraphics() noexcept;
    void ShutdownGraphics() noexcept;
    bool CreateRenderTarget() noexcept;
    void ResizeGraphics(UINT width, UINT height) noexcept;
    bool LoadDocument(const std::string& path);
    bool SaveDocument();
    bool SaveDocumentAs();
    void DrawMainWindow();
    void DrawToolbar();
    void DrawTreePanel();
    void DrawCanvasPanel();
    void DrawPropertiesPanel();
    void DrawNodeTree(XuiXmlNode& node);
    void DrawNodeProperties(XuiXmlNode& node);
    void DrawPropertyText(XuiXmlNode& node, const char* property, const char* label);
    void DrawPropertyFloat(XuiXmlNode& node, const char* property, const char* label);
    void DrawPropertyInteger(XuiXmlNode& node, const char* property, const char* label);
    void DrawPropertyBool(XuiXmlNode& node, const char* property, const char* label);
    void DrawPropertyColor(XuiXmlNode& node, const char* property, const char* label);
    void AddWidget(XuiXmlNode& parent, const char* type);
    void DeleteSelected();
    void NewDocument();
    void SetStatus(std::string status);
    static std::string OpenFileDialog(HWND owner);
    static std::string SaveFileDialog(HWND owner);
    static std::string Utf8FromWide(const wchar_t* value);
};
