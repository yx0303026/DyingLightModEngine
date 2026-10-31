#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

/* XML 节点模型概览：保留 XUI 的通用层级、属性和未知节点，编辑器只对已知布局字段提供语义化操作。 */
class XuiXmlNode final
{
public:
    std::string name;
    std::string text;
    std::map<std::string, std::string, std::less<>> attributes;
    std::vector<std::unique_ptr<XuiXmlNode>> children;
    XuiXmlNode* parent = nullptr;

    XuiXmlNode() = default;
    explicit XuiXmlNode(std::string nodeName);
    XuiXmlNode(const XuiXmlNode&) = delete;
    XuiXmlNode& operator=(const XuiXmlNode&) = delete;

    XuiXmlNode* AddChild(std::string nodeName);
    XuiXmlNode* FindChild(std::string_view nodeName) const noexcept;
    std::vector<XuiXmlNode*> FindChildren(std::string_view nodeName) const;
    XuiXmlNode* FindDescendantById(std::string_view id) const noexcept;
    XuiXmlNode* PropertiesNode() const noexcept;
    std::string GetProperty(std::string_view propertyName) const;
    bool HasProperty(std::string_view propertyName) const noexcept;
    void SetProperty(std::string_view propertyName, std::string value);
    void RemoveProperty(std::string_view propertyName) noexcept;
    bool IsWidget() const noexcept;
    std::string WidgetId() const;
    std::string WidgetText() const;
    std::string WidgetType() const;
};

/* 文档模型概览：封装 XuiCanvas 根节点、当前文件名和常用场景访问。 */
class XuiDocument final
{
public:
    XuiDocument();
    XuiDocument(const XuiDocument&) = delete;
    XuiDocument& operator=(const XuiDocument&) = delete;
    XuiDocument(XuiDocument&&) noexcept = default;
    XuiDocument& operator=(XuiDocument&&) noexcept = default;

    void Clear();
    bool IsLoaded() const noexcept;
    XuiXmlNode* Root() const noexcept;
    XuiXmlNode* Scene() const noexcept;
    const std::string& FilePath() const noexcept;
    void SetFilePath(std::string path);
    void SetRoot(std::unique_ptr<XuiXmlNode> root) noexcept;
    std::size_t CountWidgets() const noexcept;

private:
    std::unique_ptr<XuiXmlNode> root_;
    std::string filePath_;

    static std::size_t CountWidgetsRecursive(const XuiXmlNode& node) noexcept;
};
