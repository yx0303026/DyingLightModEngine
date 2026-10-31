#include "XuiEditor.h"

#include "XuiParser.h"

#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui.h>

#include <Windows.h>
#include <commdlg.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam);

namespace
{
    /* 编辑器辅助概览：集中处理 XUI 数值、路径、颜色和默认文档，保持主控制器只负责交互编排。 */
    std::string FormatFloat(float value)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(3) << value;
        return stream.str();
    }

    std::vector<float> ParseNumbers(const std::string& value)
    {
        std::vector<float> result;
        std::string current;
        for (const char character : value)
        {
            if (character == ',' || std::isspace(static_cast<unsigned char>(character)) != 0)
            {
                if (!current.empty())
                {
                    result.push_back(std::strtof(current.c_str(), nullptr));
                    current.clear();
                }
            }
            else
            {
                current.push_back(character);
            }
        }
        if (!current.empty())
        {
            result.push_back(std::strtof(current.c_str(), nullptr));
        }
        return result;
    }

    std::string FormatPosition(const ImVec2& position)
    {
        return FormatFloat(position.x) + "," + FormatFloat(position.y) + ",0.000000";
    }

}

XuiEditor::XuiEditor() = default;

XuiEditor::~XuiEditor()
{
    Shutdown();
}

bool XuiEditor::Initialize(HWND window)
{
    window_ = window;
    if (!InitializeGraphics())
    {
        return false;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0F;
    style.FrameRounding = 3.0F;
    style.ChildRounding = 3.0F;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.055F, 0.07F, 0.09F, 1.0F);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.075F, 0.09F, 0.12F, 1.0F);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12F, 0.15F, 0.19F, 1.0F);
    style.Colors[ImGuiCol_Header] = ImVec4(0.16F, 0.22F, 0.29F, 1.0F);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.2F, 0.29F, 0.38F, 1.0F);
    if (!ImGui_ImplWin32_Init(window_))
    {
        ImGui::DestroyContext();
        ShutdownGraphics();
        return false;
    }
    if (!ImGui_ImplDX11_Init(device_, deviceContext_))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        ShutdownGraphics();
        return false;
    }
    const char* chineseFontPath = "C:\\Windows\\Fonts\\msyh.ttc";
    if (std::filesystem::exists(chineseFontPath))
    {
        io.Fonts->AddFontFromFileTTF(
            chineseFontPath,
            17.0F,
            nullptr,
            io.Fonts->GetGlyphRangesChineseFull());
    }
    initialized_ = true;

    const std::filesystem::path sample(
        "E:\\SteamLibrary\\steamapps\\common\\Dying Light\\DevTools\\DW\\Data\\Menu\\Scr\\MenuMain_PC_workshop.xui");
    if (std::filesystem::exists(sample))
    {
        LoadDocument(sample.string());
    }
    else
    {
        NewDocument();
    }
    return true;
}

void XuiEditor::Shutdown() noexcept
{
    if (!initialized_)
    {
        ShutdownGraphics();
        return;
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
    ShutdownGraphics();
}

void XuiEditor::Render()
{
    if (!initialized_)
    {
        return;
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    DrawMainWindow();
    ImGui::Render();
    deviceContext_->OMSetRenderTargets(1, &renderTarget_, nullptr);
    const float clearColor[4] = { 0.025F, 0.035F, 0.05F, 1.0F };
    deviceContext_->ClearRenderTargetView(renderTarget_, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    swapChain_->Present(1, 0);
}

void XuiEditor::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
    if (!initialized_)
    {
        return;
    }
    ImGui_ImplWin32_WndProcHandler(window_, message, wParam, lParam);
    if (message == WM_SIZE && wParam != SIZE_MINIMIZED)
    {
        ResizeGraphics(LOWORD(lParam), HIWORD(lParam));
    }
}

bool XuiEditor::InitializeGraphics() noexcept
{
    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferCount = 2;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow = window_;
    description.SampleDesc.Count = 1;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        &featureLevel,
        1,
        D3D11_SDK_VERSION,
        &description,
        &swapChain_,
        &device_,
        nullptr,
        &deviceContext_)))
    {
        return false;
    }
    if (!CreateRenderTarget())
    {
        ShutdownGraphics();
        return false;
    }
    return true;
}

void XuiEditor::ShutdownGraphics() noexcept
{
    if (renderTarget_ != nullptr)
    {
        renderTarget_->Release();
        renderTarget_ = nullptr;
    }
    if (swapChain_ != nullptr)
    {
        swapChain_->SetFullscreenState(FALSE, nullptr);
        swapChain_->Release();
        swapChain_ = nullptr;
    }
    if (deviceContext_ != nullptr)
    {
        deviceContext_->Release();
        deviceContext_ = nullptr;
    }
    if (device_ != nullptr)
    {
        device_->Release();
        device_ = nullptr;
    }
}

bool XuiEditor::CreateRenderTarget() noexcept
{
    ID3D11Texture2D* backBuffer = nullptr;
    const HRESULT bufferResult = swapChain_->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(&backBuffer));
    if (FAILED(bufferResult) || backBuffer == nullptr)
    {
        return false;
    }
    const HRESULT result = device_->CreateRenderTargetView(backBuffer, nullptr, &renderTarget_);
    backBuffer->Release();
    return SUCCEEDED(result);
}

void XuiEditor::ResizeGraphics(UINT width, UINT height) noexcept
{
    if (swapChain_ == nullptr || width == 0U || height == 0U)
    {
        return;
    }
    if (renderTarget_ != nullptr)
    {
        renderTarget_->Release();
        renderTarget_ = nullptr;
    }
    if (SUCCEEDED(swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)))
    {
        CreateRenderTarget();
    }
}

bool XuiEditor::LoadDocument(const std::string& path)
{
    std::string error;
    XuiDocument loaded;
    if (!XuiParser::LoadFile(path, loaded, error))
    {
        lastError_ = error;
        SetStatus("加载失败");
        return false;
    }
    document_ = std::move(loaded);
    selectedNode_ = document_.Scene();
    lastError_.clear();
    SetStatus("已加载文档");
    return true;
}

bool XuiEditor::SaveDocument()
{
    if (!document_.IsLoaded())
    {
        return false;
    }
    std::string error;
    if (!XuiParser::SaveFile(document_.FilePath(), document_, error))
    {
        lastError_ = error;
        SetStatus("保存失败");
        return false;
    }
    lastError_.clear();
    SetStatus("已保存文档");
    return true;
}

bool XuiEditor::SaveDocumentAs()
{
    const std::string path = SaveFileDialog(window_);
    if (path.empty())
    {
        return false;
    }
    document_.SetFilePath(path);
    return SaveDocument();
}

void XuiEditor::DrawMainWindow()
{
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    ImGui::Begin(
        "Dying Light XUI 编辑器",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);
    DrawToolbar();
    ImGui::Separator();

    const float footerHeight = 26.0F;
    const float availHeight = ImGui::GetContentRegionAvail().y - footerHeight;
    const float bodyHeight = (availHeight > 100.0F) ? availHeight : 100.0F;
    if (ImGui::BeginTable("##layout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0.0F, bodyHeight)))
    {
        ImGui::TableSetupColumn("树", ImGuiTableColumnFlags_WidthFixed, 245.0F);
        ImGui::TableSetupColumn("画布", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("属性", ImGuiTableColumnFlags_WidthFixed, 330.0F);
        ImGui::TableNextColumn();
        DrawTreePanel();
        ImGui::TableNextColumn();
        DrawCanvasPanel();
        ImGui::TableNextColumn();
        DrawPropertiesPanel();
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextUnformatted(statusText_.c_str());
    if (!lastError_.empty())
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0F, 0.35F, 0.3F, 1.0F), "%s", lastError_.c_str());
    }
    ImGui::End();
}

void XuiEditor::DrawToolbar()
{
    if (ImGui::Button("新建"))
    {
        NewDocument();
    }
    ImGui::SameLine();
    if (ImGui::Button("打开"))
    {
        const std::string path = OpenFileDialog(window_);
        if (!path.empty())
        {
            LoadDocument(path);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("保存"))
    {
        if (document_.FilePath().empty())
        {
            SaveDocumentAs();
        }
        else
        {
            SaveDocument();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("另存为"))
    {
        SaveDocumentAs();
    }
    ImGui::SameLine();
    if (ImGui::Button("删除选中"))
    {
        DeleteSelected();
    }
    ImGui::SameLine();
    if (ImGui::Button("添加组"))
    {
        if (selectedNode_ != nullptr)
        {
            AddWidget(*selectedNode_, "AdvGroup");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("添加文本"))
    {
        if (selectedNode_ != nullptr)
        {
            AddWidget(*selectedNode_, "MyText");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("添加图片"))
    {
        if (selectedNode_ != nullptr)
        {
            AddWidget(*selectedNode_, "MyImage");
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("网格", &showGrid_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0F);
    float zoom = renderer_.Zoom();
    if (ImGui::SliderFloat("缩放", &zoom, 0.25F, 2.0F, "%.2fx"))
    {
        renderer_.SetZoom(zoom);
    }
}

void XuiEditor::DrawTreePanel()
{
    ImGui::BeginChild("##tree", ImVec2(0.0F, 0.0F), true);
    ImGui::TextUnformatted("场景树");
    ImGui::Separator();
    if (document_.Scene() != nullptr)
    {
        DrawNodeTree(*document_.Scene());
    }
    else
    {
        ImGui::TextDisabled("没有已加载的 XUI");
    }
    ImGui::EndChild();
}

void XuiEditor::DrawCanvasPanel()
{
    ImGui::BeginChild("##canvas", ImVec2(0.0F, 0.0F), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::Text(
        "画布 %d x %d  |  节点 %zu",
        static_cast<int>(renderer_.DesignSize(document_).x),
        static_cast<int>(renderer_.DesignSize(document_).y),
        document_.CountWidgets());
    ImGui::Separator();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    XuiXmlNode* hit = renderer_.DrawCanvas(document_, available, selectedNode_, showGrid_);
    if (hit != nullptr)
    {
        selectedNode_ = hit;
    }
    ImGui::EndChild();
}

void XuiEditor::DrawPropertiesPanel()
{
    ImGui::BeginChild("##properties", ImVec2(0.0F, 0.0F), true);
    ImGui::TextUnformatted("属性");
    ImGui::Separator();
    if (selectedNode_ == nullptr)
    {
        ImGui::TextDisabled("选择一个节点");
        ImGui::EndChild();
        return;
    }
    DrawNodeProperties(*selectedNode_);
    ImGui::EndChild();
}

void XuiEditor::DrawNodeTree(XuiXmlNode& node)
{
    if (!node.IsWidget() && node.name != "XuiScene")
    {
        for (const auto& child : node.children)
        {
            if (child != nullptr)
            {
                DrawNodeTree(*child);
            }
        }
        return;
    }
    const std::string id = node.WidgetId();
    const std::string label = id.empty() ? node.name : id + " [" + node.name + "]";
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (node.name == "XuiScene")
    {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }
    if (selectedNode_ == &node)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    bool hasChildren = false;
    for (const auto& child : node.children)
    {
        if (child != nullptr && child->IsWidget())
        {
            hasChildren = true;
            break;
        }
    }
    if (!hasChildren)
    {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    const bool open = ImGui::TreeNodeEx(
        static_cast<const void*>(&node),
        flags,
        "%s",
        label.c_str());
    if (ImGui::IsItemClicked())
    {
        selectedNode_ = &node;
    }
    if (open)
    {
        for (const auto& child : node.children)
        {
            if (child != nullptr)
            {
                DrawNodeTree(*child);
            }
        }
        ImGui::TreePop();
    }
}

void XuiEditor::DrawNodeProperties(XuiXmlNode& node)
{
    ImGui::Text("类型：%s", node.WidgetType().c_str());
    ImGui::Text("标签：%s", node.name.c_str());
    DrawPropertyText(node, "Id", "标识");
    DrawPropertyFloat(node, "Width", "宽度");
    DrawPropertyFloat(node, "Height", "高度");
    DrawPropertyInteger(node, "Anchor", "锚点");
    DrawPropertyInteger(node, "TextStyle", "文本样式");

    std::vector<float> position = ParseNumbers(node.GetProperty("Position"));
    float x = position.size() >= 1U ? position[0] : 0.0F;
    float y = position.size() >= 2U ? position[1] : 0.0F;
    if (ImGui::DragFloat2("位置", &x, 1.0F))
    {
        node.SetProperty("Position", FormatPosition(ImVec2(x, y)));
    }
    DrawPropertyFloat(node, "Opacity", "不透明度");
    DrawPropertyBool(node, "Show", "显示");
    DrawPropertyBool(node, "MultiLine", "多行文本");
    DrawPropertyText(node, "ClassOverride", "类覆盖");
    DrawPropertyText(node, "Text", "文本");
    DrawPropertyText(node, "SourceString", "HTML 源文本");
    DrawPropertyText(node, "ImagePath", "图片路径");
    DrawPropertyText(node, "Font", "字体");
    DrawPropertyText(node, "Material", "材质");
    DrawPropertyColor(node, "Color", "颜色");
    DrawPropertyColor(node, "TextColor", "文字颜色");
    DrawPropertyText(node, "Visual", "视觉样式");
    DrawPropertyText(node, "NavUp", "导航上");
    DrawPropertyText(node, "NavDown", "导航下");
    DrawPropertyText(node, "NavLeft", "导航左");
    DrawPropertyText(node, "NavRight", "导航右");
}

void XuiEditor::DrawPropertyText(XuiXmlNode& node, const char* property, const char* label)
{
    std::string value = node.GetProperty(property);
    char buffer[2048]{};
    std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputText(label, buffer, sizeof(buffer)))
    {
        node.SetProperty(property, buffer);
    }
}

void XuiEditor::DrawPropertyFloat(XuiXmlNode& node, const char* property, const char* label)
{
    std::string value = node.GetProperty(property);
    float number = value.empty() ? 0.0F : std::strtof(value.c_str(), nullptr);
    if (ImGui::DragFloat(label, &number, 0.5F, 0.0F, 4096.0F, "%.3f"))
    {
        node.SetProperty(property, FormatFloat(number));
    }
}

void XuiEditor::DrawPropertyInteger(XuiXmlNode& node, const char* property, const char* label)
{
    const std::string value = node.GetProperty(property);
    int number = value.empty() ? 0 : static_cast<int>(std::strtol(value.c_str(), nullptr, 10));
    if (ImGui::DragInt(label, &number, 1.0F, 0, 65535))
    {
        node.SetProperty(property, std::to_string(number));
    }
}

void XuiEditor::DrawPropertyBool(XuiXmlNode& node, const char* property, const char* label)
{
    std::string value = node.GetProperty(property);
    bool enabled = value.empty() || value == "true" || value == "1";
    if (ImGui::Checkbox(label, &enabled))
    {
        node.SetProperty(property, enabled ? "true" : "false");
    }
}

void XuiEditor::DrawPropertyColor(XuiXmlNode& node, const char* property, const char* label)
{
    const std::string value = node.GetProperty(property);
    unsigned long packed = value.empty() ? 0xFFFFFFFFUL : std::strtoul(value.c_str() + (value.find("0x") == std::string::npos ? 0U : 2U), nullptr, 16);
    ImVec4 color(
        static_cast<float>((packed >> 16U) & 0xFFU) / 255.0F,
        static_cast<float>((packed >> 8U) & 0xFFU) / 255.0F,
        static_cast<float>(packed & 0xFFU) / 255.0F,
        static_cast<float>((packed >> 24U) & 0xFFU) / 255.0F);
    if (ImGui::ColorEdit4(label, &color.x, ImGuiColorEditFlags_AlphaBar))
    {
        const unsigned long a = static_cast<unsigned long>(std::clamp(color.w, 0.0F, 1.0F) * 255.0F);
        const unsigned long r = static_cast<unsigned long>(std::clamp(color.x, 0.0F, 1.0F) * 255.0F);
        const unsigned long g = static_cast<unsigned long>(std::clamp(color.y, 0.0F, 1.0F) * 255.0F);
        const unsigned long b = static_cast<unsigned long>(std::clamp(color.z, 0.0F, 1.0F) * 255.0F);
        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), "0x%02lx%02lx%02lx%02lx", a, r, g, b);
        node.SetProperty(property, buffer);
    }
}

void XuiEditor::AddWidget(XuiXmlNode& parent, const char* type)
{
    const std::string baseId = type == std::string("MyText")
        ? "NewText"
        : type == std::string("MyImage") ? "NewImage" : "NewGroup";
    std::string uniqueId = baseId;
    std::uint32_t suffix = 1U;
    while (document_.Root() != nullptr &&
        document_.Root()->FindDescendantById(uniqueId) != nullptr)
    {
        uniqueId = baseId + std::to_string(suffix++);
    }

    XuiXmlNode* target = &parent;
    if ((parent.name == "MyText" || parent.name == "MyImage") && parent.parent != nullptr)
    {
        target = parent.parent;
    }
    XuiXmlNode* node = target->AddChild(type);
    node->SetProperty("Id", std::move(uniqueId));
    node->SetProperty("Width", "240.000000");
    node->SetProperty("Height", "64.000000");
    node->SetProperty("Position", "120.000000,120.000000,0.000000");
    if (type == std::string("MyText"))
    {
        node->SetProperty("Text", "新建文本");
        node->SetProperty("TextColor", "0xffffffff");
        node->SetProperty("Font", "boxed_l_10");
    }
    if (type == std::string("MyImage"))
    {
        node->SetProperty("ImagePath", "white");
        node->SetProperty("Color", "0xff445566");
    }
    selectedNode_ = node;
    SetStatus("已添加 " + std::string(type));
}

void XuiEditor::DeleteSelected()
{
    if (selectedNode_ == nullptr || selectedNode_->parent == nullptr ||
        selectedNode_->name == "XuiScene")
    {
        return;
    }
    XuiXmlNode* parent = selectedNode_->parent;
    auto iterator = parent->children.begin();
    while (iterator != parent->children.end() && iterator->get() != selectedNode_)
    {
        ++iterator;
    }
    if (iterator != parent->children.end())
    {
        parent->children.erase(iterator);
        selectedNode_ = parent;
        SetStatus("已删除节点");
    }
}

void XuiEditor::NewDocument()
{
    document_.Clear();
    document_.SetFilePath({});
    auto root = std::make_unique<XuiXmlNode>("XuiCanvas");
    root->attributes.insert_or_assign("version", "000c");
    root->SetProperty("Width", "1280.000000");
    root->SetProperty("Height", "720.000000");
    XuiXmlNode* scene = root->AddChild("XuiScene");
    scene->SetProperty("Id", "NewScene");
    scene->SetProperty("Width", "1280.000000");
    scene->SetProperty("Height", "720.000000");
    scene->SetProperty("ClassOverride", "MenuMainDI");
    XuiXmlNode* group = scene->AddChild("AdvGroup");
    group->SetProperty("Id", "MainGroup");
    group->SetProperty("Width", "1280.000000");
    group->SetProperty("Height", "720.000000");
    group->SetProperty("Position", "0.000000,0.000000,0.000000");
    selectedNode_ = scene;
    document_.SetRoot(std::move(root));
    SetStatus("已创建新文档");
}

void XuiEditor::SetStatus(std::string status)
{
    statusText_ = std::move(status);
}

std::string XuiEditor::OpenFileDialog(HWND owner)
{
    wchar_t buffer[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"XUI 文件 (*.xui)\0*.xui\0所有文件 (*.*)\0*.*\0";
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileNameW(&dialog) == TRUE ? Utf8FromWide(buffer) : std::string{};
}

std::string XuiEditor::SaveFileDialog(HWND owner)
{
    wchar_t buffer[MAX_PATH] = L"NewScreen.xui";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"XUI 文件 (*.xui)\0*.xui\0所有文件 (*.*)\0*.*\0";
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"xui";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    return GetSaveFileNameW(&dialog) == TRUE ? Utf8FromWide(buffer) : std::string{};
}

std::string XuiEditor::Utf8FromWide(const wchar_t* value)
{
    if (value == nullptr || *value == L'\0')
    {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
    result.resize(std::strlen(result.c_str()));
    return result;
}

