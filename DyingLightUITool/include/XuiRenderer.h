#pragma once

#include "XuiModel.h"

#include <imgui.h>

#include <string>

/* XUI 画布渲染概览：把 XUI 节点树投影到可缩放的 2D 设计画布，并返回鼠标命中的节点。 */
class XuiRenderer final
{
public:
    XuiRenderer();

    void SetZoom(float zoom) noexcept;
    float Zoom() const noexcept;
    ImVec2 DesignSize(const XuiDocument& document) const noexcept;
    XuiXmlNode* DrawCanvas(
        XuiDocument& document,
        const ImVec2& canvasSize,
        XuiXmlNode* selectedNode,
        bool showGrid) const;

private:
    struct Rect
    {
        ImVec2 min{};
        ImVec2 max{};
        XuiXmlNode* node = nullptr;
    };

    float zoom_ = 0.55F;
    mutable Rect hovered_{};
    mutable ImDrawList* drawList_ = nullptr;
    mutable ImVec2 origin_{};
    mutable ImVec2 canvasSize_{};
    mutable bool draggingNode_ = false;

    void DrawNode(
        XuiXmlNode& node,
        const ImVec2& parentOrigin,
        float parentWidth,
        float parentHeight,
        XuiXmlNode* selectedNode) const;
    void DrawNodeContent(
        XuiXmlNode& node,
        const Rect& rect,
        XuiXmlNode* selectedNode) const;
    static ImVec2 ReadPosition(const XuiXmlNode& node) noexcept;
    static ImVec2 ReadSize(const XuiXmlNode& node) noexcept;
    static float ReadFloat(const XuiXmlNode& node, const char* property, float fallback) noexcept;
    static bool ReadBool(const XuiXmlNode& node, const char* property, bool fallback) noexcept;
    static ImU32 ReadColor(const XuiXmlNode& node, const char* property, ImU32 fallback) noexcept;
    static bool IsTextNode(const XuiXmlNode& node) noexcept;
    static bool IsImageNode(const XuiXmlNode& node) noexcept;
    static bool IsGroupNode(const XuiXmlNode& node) noexcept;
    static bool IsDrawableNode(const XuiXmlNode& node) noexcept;
    static std::string DisplayText(const XuiXmlNode& node);
};
