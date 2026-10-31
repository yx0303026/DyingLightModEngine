#include "XuiParser.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    /* 解析测试概览：验证 XUI 属性、实体文本、层级、未知字段和时间线经过保存后仍可读取。 */
    bool Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "失败：" << message << '\n';
            return false;
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "用法：DyingLightUIToolTests <xui 路径>\n";
        return 2;
    }

    XuiDocument document;
    std::string error;
    if (!XuiParser::LoadFile(argv[1], document, error))
    {
        std::cerr << "解析失败：" << error << '\n';
        return 1;
    }

    bool passed = true;
    passed &= Expect(document.Root() != nullptr, "根节点存在");
    passed &= Expect(
        document.Root() != nullptr &&
        document.Root()->attributes.find("version") != document.Root()->attributes.end() &&
        document.Root()->attributes.at("version") == "000c",
        "根节点版本属性保留");
    passed &= Expect(document.Scene() != nullptr, "场景节点存在");
    passed &= Expect(document.CountWidgets() > 0U, "可视节点计数正确");

    XuiXmlNode* text = document.Root() == nullptr
        ? nullptr
        : document.Root()->FindDescendantById("Text_1");
    if (text == nullptr && document.Root() != nullptr)
    {
        text = document.Root()->FindDescendantById("T_GameVersion");
    }
    const std::string originalText = text == nullptr ? std::string{} : text->WidgetText();
    if (text != nullptr)
    {
        passed &= Expect(!originalText.empty(), "节点文本可读取");
        if (text->WidgetId() == "Text_1")
        {
            passed &= Expect(originalText == "Read & edit", "实体文本解码");
            passed &= Expect(text->GetProperty("NavDown") == "Button_1", "导航属性保留");
        }
    }

    const std::string serialized = XuiParser::Serialize(document);
    passed &= Expect(serialized.find("<Timelines>") != std::string::npos, "时间线被序列化");
    passed &= Expect(serialized.find("version=\"000c\"") != std::string::npos, "版本属性被序列化");
    if (text != nullptr && text->WidgetId() == "Text_1")
    {
        passed &= Expect(
            serialized.find("&Play_Custom_Map&") != std::string::npos,
            "游戏本地化占位符保持原样");
    }

    const auto uniqueSuffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path temporaryPath =
        std::filesystem::temp_directory_path() /
        ("DyingLightUIToolParserRoundTrip-" + std::to_string(uniqueSuffix) + ".xui");
    if (!XuiParser::SaveFile(temporaryPath.string(), document, error))
    {
        std::cerr << "保存失败：" << error << '\n';
        return 1;
    }
    XuiDocument reloaded;
    passed &= Expect(
        XuiParser::LoadFile(temporaryPath.string(), reloaded, error),
        "序列化结果可重新解析");
    if (reloaded.IsLoaded())
    {
        passed &= Expect(
            reloaded.CountWidgets() == document.CountWidgets(),
            "往返后节点数量一致");
        XuiXmlNode* reloadedText = reloaded.Root()->FindDescendantById("Text_1");
        if (reloadedText == nullptr)
        {
            reloadedText = reloaded.Root()->FindDescendantById("T_GameVersion");
        }
        if (!originalText.empty())
        {
            passed &= Expect(
                reloadedText != nullptr && reloadedText->WidgetText() == originalText,
                "往返后文本内容一致");
        }
        passed &= Expect(
            reloaded.Scene()->FindChild("Timelines") != nullptr ||
            reloaded.Root()->FindChild("Timelines") != nullptr,
            "往返后时间线节点一致");
    }
    std::filesystem::remove(temporaryPath);

    if (!passed)
    {
        return 1;
    }
    std::cout << "XUI 解析与往返检查通过，节点数：" << document.CountWidgets() << '\n';
    return 0;
}
