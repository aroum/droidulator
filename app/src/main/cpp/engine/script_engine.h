#pragma once

#include <string>
#include <optional>
#include <vector>
#include <algorithm>
#include <cctype>
#include "app_state.h"
#include "expression_tokens.h"

inline size_t findScriptHeaderClose(std::string_view line, size_t open) {
    if (open == std::string_view::npos) return std::string_view::npos;
    const auto close = expression::matchingGroupEnd(line, open);
    if (close == std::string_view::npos || expression::trim(line.substr(close + 1)) != "{")
        return std::string_view::npos;
    return close;
}

inline bool isBlockStart(const std::string& line) {
    if (line.empty()) return false;
    return line.back() == '{';
}

inline bool isBlockEnd(const std::string& line) {
    if (line.empty()) return false;
    return line[0] == '}' || line == "}";
}

inline size_t findMatchingBlockEnd(const std::vector<std::string>& lines, size_t start_ip, bool stop_at_else_elif = false) {
    int nesting = 1;
    for (size_t k = start_ip + 1; k < lines.size(); ++k) {
        std::string line = lines[k];
        line.erase(0, line.find_first_not_of(" \t\r"));
        line.erase(line.find_last_not_of(" \t\r") + 1);
        bool starts_with_close = (!line.empty() && line[0] == '}');
        bool ends_with_open = (!line.empty() && line.back() == '{');

        if (stop_at_else_elif && nesting == 1 && starts_with_close && ends_with_open) {
            return k;
        }

        if (starts_with_close && ends_with_open) {
            continue;
        } else if (ends_with_open) {
            nesting++;
        } else if (starts_with_close || line == "}") {
            nesting--;
            if (nesting == 0) {
                return k;
            }
        }
    }
    return std::string::npos;
}

inline std::vector<std::string> splitIntoStatements(const std::string& code, std::vector<size_t>* sourceLines = nullptr) {
    std::vector<std::string> statements;
    std::vector<size_t> newlines;
    if (sourceLines) {
        for (size_t i = 0; i < code.size(); ++i)
            if (code[i] == '\n') newlines.push_back(i);
    }
    size_t startOffset = 0;
    std::string current_stmt = "";
    bool in_quotes = false;
    
    auto push_stmt = [&](const std::string& raw) {
        std::string s = raw;
        size_t first = s.find_first_not_of(" \t\r");
        size_t last = s.find_last_not_of(" \t\r");
        if (first == std::string::npos || last == std::string::npos) return;
        s = s.substr(first, last - first + 1);
        
        auto trim = [](std::string& str) {
            str.erase(0, str.find_first_not_of(" \t\r"));
            str.erase(str.find_last_not_of(" \t\r") + 1);
        };
        auto isIdentifier = [](const std::string& name) {
            if (name.empty() || (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_')) return false;
            return std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; });
        };
        auto compoundAt = [&](const char* op) {
            const size_t pos = s.find(op);
            if (pos == std::string::npos) return std::string::npos;
            auto lhs = s.substr(0, pos); trim(lhs);
            return isIdentifier(lhs) ? pos : std::string::npos;
        };

        if (s.size() > 2 && s.substr(0, 2) == "++") {
            std::string var = s.substr(2);
            trim(var);
            s = var + " = " + var + " + 1";
        }
        else if (s.size() > 2 && s.substr(0, 2) == "--") {
            std::string var = s.substr(2);
            trim(var);
            s = var + " = " + var + " - 1";
        }
        else if (s.size() > 2 && s.substr(s.size() - 2) == "++") {
            std::string var = s.substr(0, s.size() - 2);
            trim(var);
            s = var + " = " + var + " + 1";
        }
        else if (s.size() > 2 && s.substr(s.size() - 2) == "--") {
            std::string var = s.substr(0, s.size() - 2);
            trim(var);
            s = var + " = " + var + " - 1";
        }
        else if (size_t plus_eq = compoundAt("+="); plus_eq != std::string::npos) {
            std::string var = s.substr(0, plus_eq);
            std::string val = s.substr(plus_eq + 2);
            trim(var);
            trim(val);
            s = var + " = " + var + " + (" + val + ")";
        }
        else if (size_t minus_eq = compoundAt("-="); minus_eq != std::string::npos) {
            std::string var = s.substr(0, minus_eq);
            std::string val = s.substr(minus_eq + 2);
            trim(var);
            trim(val);
            s = var + " = " + var + " - (" + val + ")";
        }
        else if (size_t mul_eq = compoundAt("*="); mul_eq != std::string::npos) {
            std::string var = s.substr(0, mul_eq);
            std::string val = s.substr(mul_eq + 2);
            trim(var);
            trim(val);
            s = var + " = " + var + " * (" + val + ")";
        }
        else if (size_t div_eq = compoundAt("/="); div_eq != std::string::npos) {
            std::string var = s.substr(0, div_eq);
            std::string val = s.substr(div_eq + 2);
            trim(var);
            trim(val);
            s = var + " = " + var + " / (" + val + ")";
        }
        
        statements.push_back(s);
        if (sourceLines)
            sourceLines->push_back(1 + (std::lower_bound(newlines.begin(), newlines.end(), startOffset) - newlines.begin()));
    };
    
    char quote_char = 0;
    int paren_depth = 0;
    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];
        if (current_stmt.empty()) startOffset = i;
        if (in_quotes && c == '\\' && i + 1 < code.size()) { current_stmt += c; current_stmt += code[++i]; continue; }
        if (c == '"' || c == '\'') {
            if (!in_quotes) {
                in_quotes = true;
                quote_char = c;
            } else if (c == quote_char) {
                in_quotes = false;
            }
            current_stmt += c;
        } else if (!in_quotes && c == '(') {
            paren_depth++;
            current_stmt += c;
        } else if (!in_quotes && c == ')') {
            if (paren_depth > 0) paren_depth--;
            current_stmt += c;
        } else if ((c == '#' || (c == '/' && i + 1 < code.size() && code[i+1] == '/')) && !in_quotes) {
            while (i < code.size() && code[i] != '\n') {
                i++;
            }
            if (!current_stmt.empty()) {
                push_stmt(current_stmt);
                current_stmt = "";
            }
        } else if (!in_quotes && paren_depth == 0 && c == '{') {
            if (!current_stmt.empty() && current_stmt.back() != ' ' && current_stmt.size() >= 4 && current_stmt.compare(current_stmt.size() - 4, 4, "else") == 0) current_stmt += ' ';
            current_stmt += c; push_stmt(current_stmt); current_stmt.clear();
        } else if (!in_quotes && paren_depth == 0 && c == '}') {
            if (!current_stmt.empty()) { push_stmt(current_stmt); current_stmt.clear(); }
            startOffset = i;
            size_t next = i + 1;
            while (next < code.size() && std::isspace(static_cast<unsigned char>(code[next]))) ++next;
            if (code.compare(next, 4, "else") == 0 || code.compare(next, 4, "elif") == 0) {
                current_stmt = "} "; i = next - 1;
            } else push_stmt("}");
        } else if (c == ';' && !in_quotes && paren_depth == 0) {
            if (!current_stmt.empty()) {
                push_stmt(current_stmt);
                current_stmt = "";
            }
        } else if (c == '\n' || c == '\r') {
            if (!current_stmt.empty()) {
                push_stmt(current_stmt);
                current_stmt = "";
            }
        } else {
            current_stmt += c;
        }
    }
    if (!current_stmt.empty()) {
        push_stmt(current_stmt);
    }
    return statements;
}


void runScript(const std::string& code);
bool executeScriptAssignments(const std::string& statements, std::string& error);
std::vector<double> evaluateScriptReturnValues(const std::string& expression, bool& isArray, std::string& error);

double executeScriptFunc(const std::string& name, const std::vector<double>& arg_vals, std::string& err, std::vector<double>* returnValues = nullptr, std::optional<std::vector<double>>* arrayResult = nullptr, const std::vector<std::optional<std::vector<double>>>* arrayArguments = nullptr);
