#include "XuiParser.h"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    /* UTF-8 路径概览：把文件对话框的 UTF-8 字节转换成 C++20 原生 Unicode 路径。 */
    std::filesystem::path PathFromUtf8(std::string_view value)
    {
        std::u8string utf8Path;
        utf8Path.reserve(value.size());
        for (const unsigned char character : value)
        {
            utf8Path.push_back(static_cast<char8_t>(character));
        }
        return std::filesystem::path(std::move(utf8Path));
    }

    /* XML 词法概览：这个轻量解析器只处理 XUI 实际使用的元素、文本和转义，不改变节点顺序。 */
    class XmlReader final
    {
    public:
        explicit XmlReader(std::string_view source)
            : source_(source)
        {
        }

        bool Read(std::unique_ptr<XuiXmlNode>& root, std::string& error)
        {
            while (!AtEnd())
            {
                SkipWhitespace();
                if (StartsWith("<?"))
                {
                    SkipUntil("?>");
                    continue;
                }
                if (StartsWith("<!--"))
                {
                    SkipUntil("-->");
                    continue;
                }
                if (Current() != '<')
                {
                    Advance();
                    continue;
                }
                root = ReadElement(nullptr, error);
                return root != nullptr;
            }
            error = "XUI 文件没有根元素";
            return false;
        }

    private:
        std::string_view source_;
        std::size_t position_ = 0;

        bool AtEnd() const noexcept
        {
            return position_ >= source_.size();
        }

        char Current() const noexcept
        {
            return AtEnd() ? '\0' : source_[position_];
        }

        void Advance() noexcept
        {
            if (!AtEnd())
            {
                ++position_;
            }
        }

        bool StartsWith(std::string_view value) const noexcept
        {
            return source_.substr(position_, value.size()) == value;
        }

        void SkipWhitespace() noexcept
        {
            while (!AtEnd() && std::isspace(static_cast<unsigned char>(Current())) != 0)
            {
                Advance();
            }
        }

        void SkipUntil(std::string_view terminator)
        {
            const std::size_t found = source_.find(terminator, position_);
            position_ = found == std::string_view::npos
                ? source_.size()
                : found + terminator.size();
        }

        std::string ReadName()
        {
            const std::size_t start = position_;
            while (!AtEnd() && (std::isalnum(static_cast<unsigned char>(Current())) != 0 ||
                Current() == '_' || Current() == '-' || Current() == ':' || Current() == '.'))
            {
                Advance();
            }
            return std::string(source_.substr(start, position_ - start));
        }

        std::string ReadText()
        {
            const std::size_t start = position_;
            while (!AtEnd() && Current() != '<')
            {
                Advance();
            }
            return Decode(source_.substr(start, position_ - start));
        }

        std::unique_ptr<XuiXmlNode> ReadElement(XuiXmlNode* parent, std::string& error)
        {
            if (Current() != '<')
            {
                error = "元素缺少起始标记";
                return nullptr;
            }
            Advance();
            if (Current() == '/')
            {
                error = "出现了多余的结束标记";
                return nullptr;
            }
            auto node = std::make_unique<XuiXmlNode>(ReadName());
            if (node->name.empty())
            {
                error = "元素名称为空";
                return nullptr;
            }
            if (!ReadAttributes(*node, error))
            {
                return nullptr;
            }
            if (StartsWith("/>"))
            {
                position_ += 2;
                node->parent = parent;
                return node;
            }
            if (Current() != '>')
            {
                error = "元素缺少结束标记";
                return nullptr;
            }
            Advance();

            std::string text;
            while (!AtEnd())
            {
                if (StartsWith("</"))
                {
                    position_ += 2;
                    const std::string closingName = ReadName();
                    SkipWhitespace();
                    if (Current() == '>')
                    {
                        Advance();
                    }
                    if (closingName != node->name)
                    {
                        error = "元素结束标记不匹配：" + node->name;
                        return nullptr;
                    }
                    node->text = TrimText(text);
                    node->parent = parent;
                    return node;
                }
                if (StartsWith("<!--"))
                {
                    SkipUntil("-->");
                    continue;
                }
                if (Current() == '<')
                {
                    std::unique_ptr<XuiXmlNode> child = ReadElement(node.get(), error);
                    if (child == nullptr)
                    {
                        return nullptr;
                    }
                    node->children.push_back(std::move(child));
                }
                else
                {
                    text += ReadText();
                }
            }
            error = "元素缺少结束标记：" + node->name;
            return nullptr;
        }

        bool ReadAttributes(XuiXmlNode& node, std::string& error)
        {
            while (!AtEnd())
            {
                SkipWhitespace();
                if (Current() == '>' || StartsWith("/>"))
                {
                    return true;
                }
                const std::string name = ReadName();
                if (name.empty())
                {
                    error = "属性名称无效：" + node.name;
                    return false;
                }
                SkipWhitespace();
                if (Current() != '=')
                {
                    error = "属性缺少等号：" + name;
                    return false;
                }
                Advance();
                SkipWhitespace();
                const char quote = Current();
                if (quote != '"' && quote != '\'')
                {
                    error = "属性值缺少引号：" + name;
                    return false;
                }
                Advance();
                const std::size_t start = position_;
                while (!AtEnd() && Current() != quote)
                {
                    Advance();
                }
                if (AtEnd())
                {
                    error = "属性值缺少结束引号：" + name;
                    return false;
                }
                node.attributes.insert_or_assign(
                    name,
                    Decode(source_.substr(start, position_ - start)));
                Advance();
            }
            error = "元素缺少结束标记：" + node.name;
            return false;
        }

        static std::string TrimText(std::string_view value)
        {
            const std::size_t first = value.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos)
            {
                return {};
            }
            const std::size_t last = value.find_last_not_of(" \t\r\n");
            return std::string(value.substr(first, last - first + 1U));
        }

        static std::string Decode(std::string_view value)
        {
            std::string result;
            result.reserve(value.size());
            for (std::size_t index = 0; index < value.size(); ++index)
            {
                if (value[index] != '&')
                {
                    result.push_back(value[index]);
                    continue;
                }
                const std::size_t semicolon = value.find(';', index + 1);
                if (semicolon == std::string_view::npos)
                {
                    result.push_back('&');
                    continue;
                }
                const std::string_view entity = value.substr(index + 1, semicolon - index - 1);
                if (entity == "amp")
                {
                    result.push_back('&');
                }
                else if (entity == "lt")
                {
                    result.push_back('<');
                }
                else if (entity == "gt")
                {
                    result.push_back('>');
                }
                else if (entity == "quot")
                {
                    result.push_back('"');
                }
                else if (entity == "apos")
                {
                    result.push_back('\'');
                }
                else
                {
                    result.append(value.substr(index, semicolon - index + 1));
                }
                index = semicolon;
            }
            return result;
        }
    };

    /* XML 输出概览：统一处理缩进、文本转义和空节点，生成游戏可以重新加载的 XUI。 */
    std::string Encode(std::string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            const char character = value[index];
            switch (character)
            {
            case '&':
            {
                const std::size_t closing = value.find('&', index + 1U);
                if (closing != std::string_view::npos && closing > index + 1U)
                {
                    const std::string_view token = value.substr(index + 1U, closing - index - 1U);
                    bool localizationToken = true;
                    for (const char tokenCharacter : token)
                    {
                        if ((std::isalnum(static_cast<unsigned char>(tokenCharacter)) == 0) &&
                            tokenCharacter != '_' && tokenCharacter != '.' && tokenCharacter != '-')
                        {
                            localizationToken = false;
                            break;
                        }
                    }
                    if (localizationToken)
                    {
                        result.append(value.substr(index, closing - index + 1U));
                        index = closing;
                    }
                    else
                    {
                        result += "&amp;";
                    }
                }
                else
                {
                    result += "&amp;";
                }
                break;
            }
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            default: result.push_back(character); break;
            }
        }
        return result;
    }

    std::string EncodeAttribute(std::string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (const char character : value)
        {
            switch (character)
            {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '"': result += "&quot;"; break;
            default: result.push_back(character); break;
            }
        }
        return result;
    }
}

bool XuiParser::LoadFile(const std::string& path, XuiDocument& document, std::string& error)
{
    std::ifstream file(PathFromUtf8(path), std::ios::binary);
    if (!file)
    {
        error = "无法打开文件：" + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (!Parse(buffer.str(), document, error))
    {
        return false;
    }
    document.SetFilePath(path);
    return true;
}

bool XuiParser::SaveFile(const std::string& path, const XuiDocument& document, std::string& error)
{
    std::ofstream file(PathFromUtf8(path), std::ios::binary | std::ios::trunc);
    if (!file)
    {
        error = "无法写入文件：" + path;
        return false;
    }
    const std::string output = Serialize(document);
    file.write(output.data(), static_cast<std::streamsize>(output.size()));
    if (!file)
    {
        error = "写入文件失败：" + path;
        return false;
    }
    return true;
}

std::string XuiParser::Serialize(const XuiDocument& document)
{
    if (!document.IsLoaded())
    {
        return {};
    }
    std::string output = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    SerializeNode(*document.Root(), output, 0U);
    return output;
}

bool XuiParser::Parse(std::string_view source, XuiDocument& document, std::string& error)
{
    XmlReader reader(source);
    std::unique_ptr<XuiXmlNode> root;
    if (!reader.Read(root, error))
    {
        return false;
    }
    document.Clear();
    document.SetRoot(std::move(root));
    return true;
}

void XuiParser::SerializeNode(
    const XuiXmlNode& node,
    std::string& output,
    std::size_t depth)
{
    output.append(depth * 2U, ' ');
    output += '<';
    output += node.name;
    for (const auto& [name, value] : node.attributes)
    {
        output += ' ';
        output += name;
        output += "=\"";
        output += EncodeAttribute(value);
        output += '"';
    }
    if (node.children.empty() && node.text.empty())
    {
        output += "/>\n";
        return;
    }
    output += ">\n";
    if (!node.text.empty())
    {
        output.append((depth + 1U) * 2U, ' ');
        output += Encode(node.text);
        output += '\n';
    }
    for (const auto& child : node.children)
    {
        if (child != nullptr)
        {
            SerializeNode(*child, output, depth + 1U);
        }
    }
    output.append(depth * 2U, ' ');
    output += "</";
    output += node.name;
    output += ">\n";
}
