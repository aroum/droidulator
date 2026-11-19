#pragma once

#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace expression {
enum class Kind { Number, ScaledNumber, Word, String, Open, Close, Comma, Operator, Other };

struct Token {
    Kind kind;
    std::string_view text;
    size_t start;
};

inline std::string_view siExponent(std::string_view suffix) {
    if (suffix == "da") return "1";
    if (suffix.size() != 1) return {};
    switch (suffix.front()) {
        case 'h': return "2";
        case 'k': return "3";
        case 'M': return "6";
        case 'G': return "9";
        case 'T': return "12";
        case 'P': return "15";
        case 'E': return "18";
        case 'Z': return "21";
        case 'Y': return "24";
        case 'd': return "-1";
        case 'c': return "-2";
        case 'm': return "-3";
        case 'u': return "-6";
        case 'n': return "-9";
        case 'p': return "-12";
        case 'f': return "-15";
        case 'a': return "-18";
        case 'z': return "-21";
        case 'y': return "-24";
        default: return {};
    }
}

inline std::vector<Token> tokenize(std::string_view source) {
    std::vector<Token> result;
    for (size_t i = 0; i < source.size();) {
        const size_t start = i;
        const unsigned char c = source[i++];
        if (std::isspace(c)) continue;
        Kind kind = Kind::Other;
        auto digit = [&](size_t pos) { return pos < source.size() && source[pos] >= '0' && source[pos] <= '9'; };
        if (digit(start) || (c == '.' && digit(i))) {
            kind = Kind::Number;
            while (digit(i)) ++i;
            if (c != '.' && i < source.size() && source[i] == '.' &&
                !(i + 1 < source.size() && (source[i + 1] == '*' || source[i + 1] == '/'))) {
                ++i;
                while (digit(i)) ++i;
            }
            const size_t mantissaEnd = i;
            if (i < source.size() && (source[i] == 'e' || source[i] == 'E')) {
                size_t exponent = i + 1;
                if (exponent < source.size() && (source[exponent] == '+' || source[exponent] == '-')) ++exponent;
                if (digit(exponent)) {
                    i = exponent + 1;
                    while (digit(i)) ++i;
                }
            }
            if (i == mantissaEnd && i < source.size()) {
                const size_t suffixLength = source.substr(i, 2) == "da" ? 2 : 1;
                const auto suffix = source.substr(i, suffixLength);
                if (!siExponent(suffix).empty()) {
                    size_t end = i + suffixLength;
                    // A suffix embedded between digits acts as the decimal point.
                    if (suffix != "da" && source.substr(start, i - start).find('.') == std::string_view::npos)
                        while (digit(end)) ++end;
                    if (end == source.size() ||
                        (!std::isalnum(static_cast<unsigned char>(source[end])) && source[end] != '_')) {
                        i = end;
                        kind = Kind::ScaledNumber;
                    }
                }
            }
        } else if (std::isalpha(c) || c == '_') {
            kind = Kind::Word;
            while (i < source.size() &&
                   (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) ++i;
        } else if (c == '"' || c == '\'') {
            kind = Kind::String;
            while (i < source.size()) {
                if (source[i++] == c) break;
                if (source[i - 1] == '\\' && i < source.size()) ++i;
            }
        } else if (c == '(' || c == '[' || c == '{') {
            kind = Kind::Open;
        } else if (c == ')' || c == ']' || c == '}') {
            kind = Kind::Close;
        } else if (c == ',') {
            kind = Kind::Comma;
        } else if (c == '.' && i < source.size() && (source[i] == '*' || source[i] == '/')) {
            kind = Kind::Operator;
            ++i;
        } else if (std::string_view("=<>!+-*/%^&|").find(c) != std::string_view::npos) {
            kind = Kind::Operator;
            if (i < source.size() &&
                ((source[i] == '=' && std::string_view("=<>!+-*/%").find(c) != std::string_view::npos) ||
                 (source[i] == c && (c == '&' || c == '|' || c == '^')))) ++i;
        }
        result.push_back({kind, source.substr(start, i - start), start});
    }
    return result;
}

inline size_t callOffset(std::string_view source, std::string_view name, size_t start = 0) {
    for (const auto& token : tokenize(source)) {
        if (token.start >= start && token.kind == Kind::Word && token.text == name &&
            token.start + name.size() < source.size() && source[token.start + name.size()] == '(')
            return token.start;
    }
    return std::string_view::npos;
}

inline bool isIdentifier(std::string_view source) {
    const auto tokens = tokenize(source);
    return tokens.size() == 1 && tokens.front().kind == Kind::Word && tokens.front().text == source;
}

inline bool closeGroup(std::vector<char>& groups, char closing) {
    const char opening = closing == ')' ? '(' : closing == ']' ? '[' : '{';
    if (groups.empty() || groups.back() != opening) return false;
    groups.pop_back();
    return true;
}

inline size_t matchingGroupEnd(std::string_view source, size_t opening) {
    std::vector<char> groups;
    for (const auto& token : tokenize(source.substr(opening))) {
        if (token.kind == Kind::Open) groups.push_back(token.text.front());
        else if (token.kind == Kind::Close) {
            if (!closeGroup(groups, token.text.front())) return std::string_view::npos;
            if (groups.empty()) return opening + token.start;
        }
    }
    return std::string_view::npos;
}

inline std::string_view trim(std::string_view source) {
    const auto first = source.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return source.substr(first, source.find_last_not_of(" \t\r\n") - first + 1);
}

inline std::vector<std::string> splitArguments(std::string_view source) {
    std::vector<std::string> arguments;
    std::vector<char> groups;
    size_t start = 0;
    for (const auto& token : tokenize(source)) {
        if (token.kind == Kind::Open) groups.push_back(token.text.front());
        else if (token.kind == Kind::Close && !closeGroup(groups, token.text.front())) return {};
        else if (token.kind == Kind::Comma && groups.empty()) {
            arguments.emplace_back(trim(source.substr(start, token.start - start)));
            start = token.start + 1;
        }
    }
    if (!groups.empty()) return {};
    if (start != source.size() || !arguments.empty())
        arguments.emplace_back(trim(source.substr(start)));
    return arguments;
}

inline size_t assignmentOffset(std::string_view source, bool compound = false) {
    std::vector<char> groups;
    for (const auto& token : tokenize(source)) {
        if (token.kind == Kind::Open) groups.push_back(token.text.front());
        else if (token.kind == Kind::Close && !closeGroup(groups, token.text.front())) break;
        else if (groups.empty() && token.kind == Kind::Operator) {
            if (token.text == "=") return token.start;
            if (compound && (token.text == "+=" || token.text == "-=" || token.text == "*=" || token.text == "/="))
                return token.start + 1;
        }
    }
    return std::string_view::npos;
}

inline std::optional<std::string_view> callBody(std::string_view source, std::string_view name) {
    const auto tokens = tokenize(source);
    if (tokens.size() < 3 || tokens[0].kind != Kind::Word || tokens[0].text != name ||
        tokens[1].text != "(") return std::nullopt;
    std::vector<char> groups;
    for (size_t i = 1; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        if (token.kind == Kind::Open) groups.push_back(token.text.front());
        if (token.kind != Kind::Close) continue;
        if (!closeGroup(groups, token.text.front())) return std::nullopt;
        if (groups.empty()) {
            if (i + 1 != tokens.size()) return std::nullopt;
            const size_t start = tokens[1].start + 1;
            return source.substr(start, token.start - start);
        }
    }
    return std::nullopt;
}
}
