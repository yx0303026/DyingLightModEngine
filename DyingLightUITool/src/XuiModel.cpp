#include "XuiModel.h"

#include <algorithm>
#include <charconv>

namespace
{
    /* 模型辅助概览：集中处理大小写无关的节点类型判断，避免编辑器各处重复字符串规则。 */
    bool Equals(std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            const char a = left[index] >= 'A' && left[index] <= 'Z'
                ? static_cast<char>(left[index] - 'A' + 'a')
                : left[index];
            const char b = right[index] >= 'A' && right[index] <= 'Z'
                ? static_cast<char>(right[index] - 'A' + 'a')
                : right[index];
            if (a != b)
            {
                return false;
            }
        }
        return true;
    }
}

XuiXmlNode::XuiXmlNode(std::string nodeName)
    : name(std::move(nodeName))
{
}

XuiXmlNode* XuiXmlNode::AddChild(std::string nodeName)
{
    auto child = std::make_unique<XuiXmlNode>(std::move(nodeName));
    child->parent = this;
    XuiXmlNode* result = child.get();
    children.push_back(std::move(child));
    return result;
}

XuiXmlNode* XuiXmlNode::FindChild(std::string_view nodeName) const noexcept
{
    for (const auto& child : children)
    {
        if (child != nullptr && child->name == nodeName)
        {
            return child.get();
        }
    }
    return nullptr;
}

std::vector<XuiXmlNode*> XuiXmlNode::FindChildren(std::string_view nodeName) const
{
    std::vector<XuiXmlNode*> result;
    for (const auto& child : children)
    {
        if (child != nullptr && child->name == nodeName)
        {
            result.push_back(child.get());
        }
    }
    return result;
}

XuiXmlNode* XuiXmlNode::FindDescendantById(std::string_view id) const noexcept
{
    if (WidgetId() == id)
    {
        return const_cast<XuiXmlNode*>(this);
    }
    for (const auto& child : children)
    {
        if (child != nullptr)
        {
            XuiXmlNode* result = child->FindDescendantById(id);
            if (result != nullptr)
            {
                return result;
            }
        }
    }
    return nullptr;
}

XuiXmlNode* XuiXmlNode::PropertiesNode() const noexcept
{
    return FindChild("Properties");
}

std::string XuiXmlNode::GetProperty(std::string_view propertyName) const
{
    const XuiXmlNode* properties = PropertiesNode();
    if (properties == nullptr)
    {
        return {};
    }
    const XuiXmlNode* property = properties->FindChild(propertyName);
    return property == nullptr ? std::string{} : property->text;
}

bool XuiXmlNode::HasProperty(std::string_view propertyName) const noexcept
{
    const XuiXmlNode* properties = PropertiesNode();
    return properties != nullptr && properties->FindChild(propertyName) != nullptr;
}

void XuiXmlNode::SetProperty(std::string_view propertyName, std::string value)
{
    XuiXmlNode* properties = PropertiesNode();
    if (properties == nullptr)
    {
        properties = const_cast<XuiXmlNode*>(this)->AddChild("Properties");
    }
    XuiXmlNode* property = properties->FindChild(propertyName);
    if (property == nullptr)
    {
        property = properties->AddChild(std::string(propertyName));
    }
    property->text = std::move(value);
}

void XuiXmlNode::RemoveProperty(std::string_view propertyName) noexcept
{
    XuiXmlNode* properties = PropertiesNode();
    if (properties == nullptr)
    {
        return;
    }
    auto iterator = properties->children.begin();
    while (iterator != properties->children.end() &&
        ((*iterator == nullptr) || (*iterator)->name != propertyName))
    {
        ++iterator;
    }
    if (iterator != properties->children.end())
    {
        properties->children.erase(iterator);
    }
}

bool XuiXmlNode::IsWidget() const noexcept
{
    return name != "XuiCanvas" &&
        name != "XuiScene" &&
        name != "Properties" &&
        name != "Timelines" &&
        name != "NamedFrames" &&
        name != "NamedFrame" &&
        name != "Timeline" &&
        name != "KeyFrame" &&
        PropertiesNode() != nullptr;
}

std::string XuiXmlNode::WidgetId() const
{
    return GetProperty("Id");
}

std::string XuiXmlNode::WidgetText() const
{
    std::string textValue = GetProperty("Text");
    if (textValue.empty())
    {
        textValue = GetProperty("SourceString");
    }
    return textValue;
}

std::string XuiXmlNode::WidgetType() const
{
    const std::string classOverride = GetProperty("ClassOverride");
    return classOverride.empty() ? name : classOverride;
}

XuiDocument::XuiDocument() = default;

void XuiDocument::Clear()
{
    root_.reset();
    filePath_.clear();
}

bool XuiDocument::IsLoaded() const noexcept
{
    return root_ != nullptr;
}

XuiXmlNode* XuiDocument::Root() const noexcept
{
    return root_.get();
}

XuiXmlNode* XuiDocument::Scene() const noexcept
{
    return root_ == nullptr ? nullptr : root_->FindChild("XuiScene");
}

const std::string& XuiDocument::FilePath() const noexcept
{
    return filePath_;
}

void XuiDocument::SetFilePath(std::string path)
{
    filePath_ = std::move(path);
}

void XuiDocument::SetRoot(std::unique_ptr<XuiXmlNode> root) noexcept
{
    root_ = std::move(root);
}

std::size_t XuiDocument::CountWidgets() const noexcept
{
    return root_ == nullptr ? 0U : CountWidgetsRecursive(*root_);
}

std::size_t XuiDocument::CountWidgetsRecursive(const XuiXmlNode& node) noexcept
{
    std::size_t count = node.IsWidget() ? 1U : 0U;
    for (const auto& child : node.children)
    {
        if (child != nullptr)
        {
            count += CountWidgetsRecursive(*child);
        }
    }
    return count;
}
