#include "XuiRenderer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace
{
    /* 画布解析概览：读取 XUI 的逗号分隔数值和颜色表示，给渲染器提供稳定的默认值。 */
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

    bool Contains(std::string_view value, std::string_view part) noexcept
    {
        return value.find(part) != std::string_view::npos;
    }
}

XuiRenderer::XuiRenderer() = default;

void XuiRenderer::SetZoom(float zoom) noexcept
{
    zoom_ = std::clamp(zoom, 0.25F, 2.0F);
}

float XuiRenderer::Zoom() const noexcept
{
    return zoom_;
}

ImVec2 XuiRenderer::DesignSize(const XuiDocument& document) const noexcept
{
    const XuiXmlNode* scene = document.Scene();
    if (scene == nullptr)
    {
        return ImVec2(1280.0F, 720.0F);
    }
    return ImVec2(
        ReadFloat(*scene, "Width", 1280.0F),
        ReadFloat(*scene, "Height", 720.0F));
}

XuiXmlNode* XuiRenderer::DrawCanvas(
    XuiDocument& document,
    const ImVec2& canvasSize,
    XuiXmlNode* selectedNode,
    bool showGrid) const
{
    drawList_ = ImGui::GetWindowDrawList();
    canvasSize_ = canvasSize;
    const ImVec2 designSize = DesignSize(document);
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    origin_ = ImVec2(
        cursor.x + std::max(0.0F, (canvasSize.x - designSize.x * zoom_) * 0.5F),
        cursor.y + std::max(0.0F, (canvasSize.y - designSize.y * zoom_) * 0.5F));
    const ImVec2 canvasEnd(
        origin_.x + designSize.x * zoom_,
        origin_.y + designSize.y * zoom_);
    drawList_->AddRectFilled(
        cursor,
        ImVec2(cursor.x + canvasSize.x, cursor.y + canvasSize.y),
        IM_COL32(24, 28, 34, 255));
    drawList_->AddRectFilled(origin_, canvasEnd, IM_COL32(46, 50, 58, 255));

    if (document.Scene() == nullptr)
    {
        drawList_->AddText(
            ImVec2(cursor.x + 24.0F, cursor.y + 24.0F),
            IM_COL32(180, 190, 202, 255),
            "打开一个 XUI 文件开始编辑");
        ImGui::Dummy(canvasSize);
        return nullptr;
    }

    if (showGrid)
    {
        const float gridStep = 40.0F * zoom_;
        for (float x = origin_.x; x <= canvasEnd.x; x += gridStep)
        {
            drawList_->AddLine(
                ImVec2(x, origin_.y),
                ImVec2(x, canvasEnd.y),
                IM_COL32(70, 75, 84, 90));
        }
        for (float y = origin_.y; y <= canvasEnd.y; y += gridStep)
        {
            drawList_->AddLine(
                ImVec2(origin_.x, y),
                ImVec2(canvasEnd.x, y),
                IM_COL32(70, 75, 84, 90));
        }
    }

    hovered_ = {};
    for (const auto& child : document.Scene()->children)
    {
        if (child != nullptr && child->IsWidget())
        {
            DrawNode(*child, origin_, designSize.x, designSize.y, selectedNode);
        }
    }
    drawList_->AddRect(origin_, canvasEnd, IM_COL32(170, 180, 195, 220), 0.0F, 0, 1.0F);
    ImGui::InvisibleButton("##xui_canvas", canvasSize);
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        draggingNode_ = hovered_.node != nullptr;
        return hovered_.node;
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        draggingNode_ = false;
    }
    if (draggingNode_ && ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
        selectedNode != nullptr && selectedNode->IsWidget())
    {
        const ImVec2 delta(
            ImGui::GetIO().MouseDelta.x / zoom_,
            ImGui::GetIO().MouseDelta.y / zoom_);
        const ImVec2 position = ReadPosition(*selectedNode);
        selectedNode->SetProperty(
            "Position",
            std::to_string(position.x + delta.x) + "," +
            std::to_string(position.y + delta.y) + ",0.000000");
    }
    return selectedNode;
}

void XuiRenderer::DrawNode(
    XuiXmlNode& node,
    const ImVec2& parentOrigin,
    float parentWidth,
    float parentHeight,
    XuiXmlNode* selectedNode) const
{
    if (!ReadBool(node, "Show", true))
    {
        return;
    }
    const ImVec2 position = ReadPosition(node);
    const ImVec2 size = ReadSize(node);
    const ImVec2 min(
        parentOrigin.x + position.x * zoom_,
        parentOrigin.y + position.y * zoom_);
    const ImVec2 max(
        min.x + size.x * zoom_,
        min.y + size.y * zoom_);
    const Rect rect{ min, max, &node };
    if (ImGui::IsMouseHoveringRect(min, max))
    {
        hovered_ = rect;
    }
    DrawNodeContent(node, rect, selectedNode);

    for (const auto& child : node.children)
    {
        if (child != nullptr && child->IsWidget())
        {
            DrawNode(*child, min, size.x > 0.0F ? size.x : parentWidth, size.y > 0.0F ? size.y : parentHeight, selectedNode);
        }
    }
}

void XuiRenderer::DrawNodeContent(
    XuiXmlNode& node,
    const Rect& rect,
    XuiXmlNode* selectedNode) const
{
    if (!IsDrawableNode(node))
    {
        return;
    }
    const float opacity = std::clamp(ReadFloat(node, "Opacity", 1.0F), 0.0F, 1.0F);
    const bool selected = selectedNode == &node;
    ImU32 color = IM_COL32(92, 112, 138, static_cast<int>(100.0F * opacity));
    if (IsTextNode(node))
    {
        color = ReadColor(node, "TextColor", IM_COL32(232, 235, 238, static_cast<int>(255.0F * opacity)));
        drawList_->AddText(
            ImVec2(rect.min.x + 4.0F, rect.min.y + 4.0F),
            color,
            DisplayText(node).c_str());
    }
    else if (IsImageNode(node))
    {
        color = ReadColor(node, "Color", IM_COL32(120, 135, 155, static_cast<int>(150.0F * opacity)));
        drawList_->AddRectFilled(rect.min, rect.max, color, 2.0F);
        const std::string path = node.GetProperty("ImagePath");
        if (!path.empty())
        {
            drawList_->AddText(
                ImVec2(rect.min.x + 4.0F, rect.min.y + 4.0F),
                IM_COL32(230, 235, 240, 220),
                path.c_str());
        }
    }
    else
    {
        drawList_->AddRect(
            rect.min,
            rect.max,
            color,
            2.0F,
            0,
            selected ? 2.0F : 1.0F);
    }
    if (selected)
    {
        drawList_->AddRect(
            ImVec2(rect.min.x - 2.0F, rect.min.y - 2.0F),
            ImVec2(rect.max.x + 2.0F, rect.max.y + 2.0F),
            IM_COL32(255, 181, 71, 255),
            2.0F,
            0,
            2.0F);
    }
}

ImVec2 XuiRenderer::ReadPosition(const XuiXmlNode& node) noexcept
{
    const std::vector<float> values = ParseNumbers(node.GetProperty("Position"));
    return values.size() >= 2U ? ImVec2(values[0], values[1]) : ImVec2(0.0F, 0.0F);
}

ImVec2 XuiRenderer::ReadSize(const XuiXmlNode& node) noexcept
{
    const std::vector<float> values = ParseNumbers(node.GetProperty("Size"));
    if (values.size() >= 2U)
    {
        return ImVec2(values[0], values[1]);
    }
    return ImVec2(
        ReadFloat(node, "Width", 80.0F),
        ReadFloat(node, "Height", 32.0F));
}

float XuiRenderer::ReadFloat(const XuiXmlNode& node, const char* property, float fallback) noexcept
{
    const std::string value = node.GetProperty(property);
    if (value.empty())
    {
        return fallback;
    }
    return std::strtof(value.c_str(), nullptr);
}

bool XuiRenderer::ReadBool(const XuiXmlNode& node, const char* property, bool fallback) noexcept
{
    const std::string value = node.GetProperty(property);
    if (value.empty())
    {
        return fallback;
    }
    return value == "true" || value == "1" || value == "TRUE";
}

ImU32 XuiRenderer::ReadColor(const XuiXmlNode& node, const char* property, ImU32 fallback) noexcept
{
    const std::string value = node.GetProperty(property);
    if (value.size() < 3U)
    {
        return fallback;
    }
    const std::size_t start = value.find("0x");
    if (start == std::string::npos)
    {
        return fallback;
    }
    const unsigned long color = std::strtoul(value.c_str() + start + 2U, nullptr, 16);
    const unsigned char a = static_cast<unsigned char>((color >> 24U) & 0xFFU);
    const unsigned char r = static_cast<unsigned char>((color >> 16U) & 0xFFU);
    const unsigned char g = static_cast<unsigned char>((color >> 8U) & 0xFFU);
    const unsigned char b = static_cast<unsigned char>(color & 0xFFU);
    return IM_COL32(r, g, b, a == 0U ? 255U : a);
}

bool XuiRenderer::IsTextNode(const XuiXmlNode& node) noexcept
{
    return Contains(node.name, "Text") || Contains(node.WidgetType(), "Text");
}

bool XuiRenderer::IsImageNode(const XuiXmlNode& node) noexcept
{
    return Contains(node.name, "Image") ||
        Contains(node.WidgetType(), "Image") ||
        node.HasProperty("ImagePath");
}

bool XuiRenderer::IsGroupNode(const XuiXmlNode& node) noexcept
{
    return Contains(node.name, "Group") || Contains(node.WidgetType(), "Group");
}

bool XuiRenderer::IsDrawableNode(const XuiXmlNode& node) noexcept
{
    return IsTextNode(node) || IsImageNode(node) || IsGroupNode(node) ||
        node.name.find("Button") != std::string::npos ||
        node.HasProperty("Color") ||
        node.HasProperty("Material");
}

std::string XuiRenderer::DisplayText(const XuiXmlNode& node)
{
    std::string text = node.WidgetText();
    std::replace(text.begin(), text.end(), '\n', ' ');
    if (text.size() > 44U)
    {
        text.resize(44U);
        text += "...";
    }
    return text.empty() ? node.WidgetId() : text;
}
