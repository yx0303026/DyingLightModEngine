#pragma once

#include "XuiModel.h"

#include <string>

/* XUI 文件服务概览：负责 UTF-8 XML 的加载、解析、格式化输出和文件保存。 */
class XuiParser final
{
public:
    static bool LoadFile(const std::string& path, XuiDocument& document, std::string& error);
    static bool SaveFile(const std::string& path, const XuiDocument& document, std::string& error);
    static std::string Serialize(const XuiDocument& document);

private:
    static bool Parse(std::string_view source, XuiDocument& document, std::string& error);
    static void SerializeNode(
        const XuiXmlNode& node,
        std::string& output,
        std::size_t depth);
};
