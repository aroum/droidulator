#include "script_engine.h"
#include "math_funcs.h"
#include "math_engine.h"
#include "plot_engine.h"
#include <iostream>
#include <string_view>
#include <unordered_set>

extern double evaluate(const std::string& expr_str, std::string& err);
extern double evaluateInput(const std::string& input, std::string& err, bool& isDefinition);
extern std::string preprocessArrayLookups(const std::string& expr, std::string& err);
extern std::string formatPrintString(const std::string& str, std::string& err);
extern bool parseArrayElementAssignment(const std::string& line, std::string& name, std::string& idx_expr, std::string& val_expr);
extern std::vector<double> parseArrayExpr(const std::string& rhs, bool& is_array, std::string& err);
extern std::string preprocessLen(const std::string& s);


// History values are read-only in every script assignment, including for headers.
static bool writesHistoryVariable(const std::string& code) {
    bool in_comment = false;
    char quote = 0;
    for (size_t i = 0; i < code.size(); ++i) {
        const char c = code[i];
        if (in_comment) { if (c == '\n') in_comment = false; continue; }
        if (quote) {
            if (c == '\\' && i + 1 < code.size()) ++i;
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == '\'' || c == '"') { quote = c; continue; }
        if (c == '#' || (c == '/' && i + 1 < code.size() && code[i + 1] == '/')) { in_comment = true; continue; }
        if (c != 'e' || (i && (std::isalnum(static_cast<unsigned char>(code[i - 1])) || code[i - 1] == '_'))) continue;
        size_t end = i + 1;
        while (end < code.size() && std::isdigit(static_cast<unsigned char>(code[end]))) ++end;
        if (end == i + 1 || (end < code.size() && (std::isalnum(static_cast<unsigned char>(code[end])) || code[end] == '_'))) continue;
        size_t op = end;
        while (op < code.size() && std::isspace(static_cast<unsigned char>(code[op]))) ++op;
        if (op >= code.size()) continue;
        if (code[op] == '=' && (op + 1 >= code.size() || code[op + 1] != '=')) return true;
        if (op + 1 < code.size() && ((code[op] == '+' || code[op] == '-') && code[op + 1] == code[op])) return true;
        if (op + 1 < code.size() && (code[op] == '+' || code[op] == '-' || code[op] == '*' || code[op] == '/') && code[op + 1] == '=') return true;
    }
    return false;
}

static std::string reservedAssignment(const std::string& code) {
    char quote = 0;
    bool comment = false;
    for (size_t i = 0; i < code.size();) {
        const char c = code[i];
        if (comment) { if (c == '\n') comment = false; ++i; continue; }
        if (quote) { if (c == '\\' && i + 1 < code.size()) i += 2; else { if (c == quote) quote = 0; ++i; } continue; }
        if (c == '\'' || c == '"') { quote = c; ++i; continue; }
        if (c == '#' || (c == '/' && i + 1 < code.size() && code[i + 1] == '/')) { comment = true; ++i; continue; }
        if (!std::isalpha(static_cast<unsigned char>(c)) && c != '_') { ++i; continue; }
        const size_t start = i++;
        while (i < code.size() && (std::isalnum(static_cast<unsigned char>(code[i])) || code[i] == '_')) ++i;
        const auto name = code.substr(start, i - start);
        if (!isReservedName(name)) continue;
        size_t op = i;
        while (op < code.size() && std::isspace(static_cast<unsigned char>(code[op]))) ++op;
        if (op >= code.size()) continue;
        if (code[op] == '=' && (op + 1 >= code.size() || code[op + 1] != '=')) return name;
        if (op + 1 < code.size() && ((code[op] == '+' || code[op] == '-') && code[op + 1] == code[op])) return name;
        if (op + 1 < code.size() && (code[op] == '+' || code[op] == '-' || code[op] == '*' || code[op] == '/') && code[op + 1] == '=') return name;
    }
    return {};
}

static bool evaluateScriptCondition(const std::string& condition, std::string& error) {
    const auto expression = preprocessArrayLookups(condition, error);
    if (!error.empty()) return false;
    const double value = evaluateScalar(expression, error, "Condition");
    return error.empty() && !std::isnan(value) && value != 0.0;
}

static size_t afterScriptBlock(const std::vector<std::string>& lines, size_t start) {
    const auto end = findMatchingBlockEnd(lines, start);
    return end == std::string::npos ? lines.size() : end + 1;
}

static bool handleScriptFunctionDeclaration(
    const std::vector<std::string>& lines, size_t sourceLine, size_t& ip
) {
    const auto& line = lines[ip];
    size_t keywordLength;
    if (line.rfind("fn ", 0) == 0) keywordLength = 3;
    else if (line.rfind("def ", 0) == 0) keywordLength = 4;
    else if (line.rfind("function ", 0) == 0) keywordLength = 9;
    else return false;

    const size_t open = line.find('(');
    const size_t close = open == std::string::npos ? std::string::npos : expression::matchingGroupEnd(line, open);
    if (open == std::string::npos || close == std::string::npos) {
        engineState().script_console_output.push_back("L:" + std::to_string(sourceLine) + " Err: Invalid function signature");
        const auto end = findMatchingBlockEnd(lines, ip);
        ip = end == std::string::npos ? lines.size() : end + 1;
        return true;
    }

    const auto trim = [](std::string text) {
        text.erase(0, text.find_first_not_of(" \t"));
        text.erase(text.find_last_not_of(" \t") + 1);
        return text;
    };
    const auto name = trim(line.substr(keywordLength, open - keywordLength));
    const auto parameterText = line.substr(open + 1, close - open - 1);
    const auto params = expression::splitArguments(parameterText);


    auto& state = engineState();
    size_t end = findMatchingBlockEnd(lines, ip);
    std::string error;
    const auto suffix = expression::trim(std::string_view(line).substr(close + 1));
    if (!suffix.empty() && suffix != "{") error = "Invalid function signature";
    else if (!expression::trim(parameterText).empty() && params.empty()) error = "Invalid parameter list";
    else if (!expression::isIdentifier(name)) error = "Invalid function name";
    else if (std::any_of(params.begin(), params.end(), [&](const auto& param) { return !expression::isIdentifier(param); }))
        error = "Invalid parameter name";
    else if (std::unordered_set<std::string>(params.begin(), params.end()).size() != params.size())
        error = "Duplicate parameter";
    else if (isReservedName(name)) error = "Reserved name: " + name;
    else if (std::any_of(params.begin(), params.end(), isReservedName)) error = "Reserved parameter";
    if (!error.empty()) {
        state.script_console_output.push_back("L:" + std::to_string(sourceLine) + " Err: " + error);
        ip = end == std::string::npos ? lines.size() : end + 1;
        return true;
    }

    // Declarations without braces can also terminate with an explicit end.
    if (end == std::string::npos) {
        end = ip + 1;
        while (end < lines.size() && lines[end] != "end") ++end;
    }
    if (end == lines.size()) {
        state.script_console_output.push_back("L:" + std::to_string(sourceLine) + " Err: Unclosed function body");
        ip = lines.size();
        return true;
    }
    std::vector<std::string> body(lines.begin() + ip + 1, lines.begin() + end);
    ip = end + 1;
    auto& functions = state.user_script_funcs;
    functions.erase(std::remove_if(functions.begin(), functions.end(), [&](const CustomScriptFunc& function) {
        return function.name == name;
    }), functions.end());
    functions.push_back({name, std::move(params), std::move(body)});
    auto& words = state.autocomplete_words;
    const auto completion = name + "(";
    if (std::find(words.begin(), words.end(), completion) == words.end()) words.push_back(completion);
    return true;
}

std::vector<double> evaluateScriptReturnValues(const std::string& text, bool& isArray, std::string& error) {
    error.clear();
    isArray = false;
    const std::string expressionText = expression::trim(text).empty() ? "0" : std::string(expression::trim(text));
    const auto parts = expression::splitArguments(expressionText);
    if (parts.empty()) {
        error = "Invalid return expression";
        return {};
    }
    if (parts.size() > 1) {
        isArray = true;
        std::vector<double> values;
        values.reserve(parts.size());
        for (const auto& part : parts) {
            if (part.empty()) {
                error = "Empty return value";
                return {};
            }
            const double value = evaluate(part, error);
            if (!error.empty()) return {};
            values.push_back(value);
        }
        return values;
    }
    auto values = parseArrayExpr(expressionText, isArray, error);
    if (!error.empty() || isArray || !values.empty()) return values;
    const double value = evaluate(expressionText, error);
    return error.empty() ? std::vector<double>{value} : std::vector<double>{};
}

static void evaluateScriptReturn(std::string expressionText, size_t sourceLine) {
    auto& state = engineState();
    std::string error;
    bool isArray = false;
    auto values = evaluateScriptReturnValues(expressionText, isArray, error);

    state.script_return_val = values.empty() ? 0.0 : values.front();
    state.script_return_vec = std::move(values);
    state.script_return_is_vec = isArray;
    state.script_has_returned = true;
    if (!error.empty()) {
        state.script_console_output.push_back("L:" + std::to_string(sourceLine) + " Err: " + error);
    } else if (isArray) {
        std::string formatted = "Return: [";
        for (size_t i = 0; i < state.script_return_vec.size(); ++i) {
            if (i != 0) formatted += ", ";
            formatted += fmtNum(state.script_return_vec[i]);
        }
        state.script_console_output.push_back(formatted + "]");
    } else {
        state.script_console_output.push_back("Return: " + fmtNum(state.script_return_val));
    }
}

static bool isScriptConstant(const std::string& name) {
    const auto& constants = engineState().user_consts;
    return std::any_of(constants.begin(), constants.end(), [&](const UserConst& value) {
        return value.name == name;
    });
}

static bool assignScriptTuple(const std::string& lhs, const std::string& rhs, std::string& error) {
    const auto names = expression::splitArguments(lhs);
    if (names.empty() || std::any_of(names.begin(), names.end(), [](const auto& name) {
        return !expression::isIdentifier(name);
    })) {
        error = "Invalid tuple assignment";
        return false;
    }
    if (std::unordered_set<std::string>(names.begin(), names.end()).size() != names.size()) {
        error = "Duplicate tuple target";
        return false;
    }
    for (const auto& name : names) {
        if (isReservedName(name) || isScriptConstant(name)) {
            error = "Const or reserved name";
            return false;
        }
    }
    std::vector<double> values;
    bool calledFunction = false;
    const size_t open = rhs.find('(');
    if (open != std::string::npos && expression::matchingGroupEnd(rhs, open) == rhs.size() - 1) {
        const std::string name(expression::trim(std::string_view(rhs).substr(0, open)));
        const auto& functions = engineState().user_script_funcs;
        const auto function = std::find_if(functions.begin(), functions.end(), [&](const auto& candidate) { return candidate.name == name; });
        if (function != functions.end()) {
            calledFunction = true;
            const auto text = std::string_view(rhs).substr(open + 1, rhs.size() - open - 2);
            const auto expressions = expression::splitArguments(text);
            if (!expression::trim(text).empty() && expressions.empty()) {
                error = "Invalid function arguments";
                return false;
            }
            if (expressions.size() != function->params.size()) {
                error = "Arg Count Mismatch";
                return false;
            }
            std::vector<double> arguments;
            arguments.reserve(expressions.size());
            for (const auto& argument : expressions) {
                if (argument.empty()) {
                    error = "Empty function argument";
                    return false;
                }
                arguments.push_back(evaluateScalar(argument, error, "Function argument"));
                if (!error.empty()) return false;
            }
            executeScriptFunc(name, arguments, error, &values);
        }
    }
    if (!calledFunction) {
        bool array = false;
        values = evaluateScriptReturnValues(rhs, array, error);
    }
    if (!error.empty()) return false;
    auto& state = engineState();
    for (size_t i = 0; i < names.size(); ++i) {
        const auto& name = names[i];
        const double value = i < values.size() ? values[i] : 0.0;
        state.user_arrays.erase(name);
        const auto target = std::find_if(state.user_args.begin(), state.user_args.end(), [&](const UserArg& argument) {
            return argument.name == name;
        });
        if (target == state.user_args.end()) state.user_args.push_back({name, value});
        else target->val = value;
        if (std::find(state.autocomplete_words.begin(), state.autocomplete_words.end(), name) == state.autocomplete_words.end())
            state.autocomplete_words.push_back(name);
    }
    return true;
}

bool executeScriptAssignments(const std::string& step, std::string& err) {
    if (step.empty()) return true;
    bool isDef = false;
    std::vector<std::string> step_stmts = splitIntoStatements(step);
    for (auto st : step_stmts) {
        st.erase(0, st.find_first_not_of(" \t"));
        st.erase(st.find_last_not_of(" \t") + 1);
        if (st.size() > 2 && (st.substr(0, 2) == "++" || st.substr(0, 2) == "--")) {
            st = std::string(expression::trim(std::string_view(st).substr(2))) + st.substr(0, 2);
        }
        std::string arrayName, indexExpression, valueExpression;
        if (parseArrayElementAssignment(st, arrayName, indexExpression, valueExpression)) {
            assignArrayElement(arrayName, indexExpression, valueExpression, err);
        } else if (st.size() > 2 && (st.substr(st.size() - 2) == "++" || st.substr(st.size() - 2) == "--")) {
            std::string var_name = st.substr(0, st.size() - 2);
            var_name.erase(0, var_name.find_first_not_of(" \t"));
            var_name.erase(var_name.find_last_not_of(" \t") + 1);
            if (!expression::isIdentifier(var_name)) {
                err = "Invalid assignment name";
                return false;
            }
            if (isReservedName(var_name)) {
                err = "Reserved name: " + var_name;
                return false;
            }
            if (isScriptConstant(var_name)) {
                err = "Const Error";
                return false;
            }
            engineState().user_arrays.erase(var_name);
            char op = st[st.size() - 1];
            bool found = false;
            for (auto& arg : engineState().user_args) {
                if (arg.name == var_name) {
                    if (op == '+') arg.val += 1.0;
                    else arg.val -= 1.0;
                    found = true;
                    break;
                }
            }
            if (!found) {
                engineState().user_args.push_back({var_name, op == '+' ? 1.0 : -1.0});
            }
        } else if (const size_t eq = expression::assignmentOffset(st, true); eq != std::string::npos) {
            std::string lhs = st.substr(0, eq);
            std::string rhs = st.substr(eq + 1);
            if (!lhs.empty() && (lhs.back() == '+' || lhs.back() == '-' || lhs.back() == '*' || lhs.back() == '/')) {
                char op = lhs.back();
                lhs = lhs.substr(0, lhs.size() - 1);
                rhs = lhs + " " + std::string(1, op) + " (" + rhs + ")";
            }
            auto trim = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \t"));
                s.erase(s.find_last_not_of(" \t") + 1);
            };
            trim(lhs); trim(rhs);
            if (!expression::isIdentifier(lhs)) {
                err = "Invalid assignment name";
                return false;
            }
            if (isReservedName(lhs)) {
                err = "Reserved name: " + lhs;
                return false;
            }
            if (isScriptConstant(lhs)) {
                err = "Const Error";
                return false;
            }
            const auto expanded = preprocessArrayLookups(rhs, err);
            if (!err.empty()) return false;
            double val = evaluate(preprocessLen(expanded), err);
            if (err.empty()) {
                engineState().user_arrays.erase(lhs);
                bool found = false;
                for (auto& a : engineState().user_args) {
                    if (a.name == lhs) {
                        a.val = val;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    engineState().user_args.push_back({lhs, val});
                }
            }
        } else {
            const auto expanded = preprocessArrayLookups(st, err);
            if (!err.empty()) return false;
            evaluateInput(preprocessLen(expanded), err, isDef);
        }
        if (!err.empty()) return false;
    }
    return true;
}

double executeScriptFunc(const std::string& name, const std::vector<double>& arg_vals, std::string& err, std::vector<double>* returnValues) {
    static thread_local unsigned depth = 0;
    if (depth >= 64) { err = "Function recursion limit exceeded"; return std::numeric_limits<double>::quiet_NaN(); }
    struct DepthGuard { unsigned& value; explicit DepthGuard(unsigned& d) : value(d) { ++value; } ~DepthGuard() { --value; } } guard(depth);
    auto it = std::find_if(engineState().user_script_funcs.begin(), engineState().user_script_funcs.end(), [&](const CustomScriptFunc& f) {
        return f.name == name;
    });
    if (it == engineState().user_script_funcs.end()) {
        err = "Func Not Found";
        return std::numeric_limits<double>::quiet_NaN();
    }

    const auto func = *it;
    if (arg_vals.size() != func.params.size()) {
        err = "Arg Count Mismatch";
        return std::numeric_limits<double>::quiet_NaN();
    }

    auto orig_args = engineState().user_args;
    auto orig_arrays = engineState().user_arrays;

    for (size_t i = 0; i < func.params.size(); ++i) {
        engineState().user_args.erase(std::remove_if(engineState().user_args.begin(), engineState().user_args.end(), [&](const UserArg& a) {
            return a.name == func.params[i];
        }), engineState().user_args.end());
        engineState().user_args.push_back({func.params[i], arg_vals[i]});
    }

    double ret_val = 0.0;
    bool has_returned = false;
    int step_count = 0;
    int max_steps = 1000000;
    int loopIterations = 0;

    const auto& lines = func.statements;
    size_t ip = 0;
    struct FnBlock {
        std::string type;
        bool condition_met;
        size_t start_ip;
        size_t end_ip;
        std::string cond_str;
        std::string step_str;
    };
    std::vector<FnBlock> blocks;

    while (ip < lines.size() && step_count < max_steps) {
        if (has_returned) break;
        step_count++;
        std::string line = lines[ip];
        line.erase(0, line.find_first_not_of(" \t\r"));
        line.erase(line.find_last_not_of(" \t\r") + 1);
        if (line.empty() || line[0] == '#' || line.rfind("//", 0) == 0) { ip++; continue; }

        if (line == "break" || line == "continue") {
            auto loop = std::find_if(blocks.rbegin(), blocks.rend(), [](const FnBlock& b) { return b.type == "for" || b.type == "while"; });
            if (loop == blocks.rend()) { err = line + " outside loop"; break; }
            const size_t index = static_cast<size_t>(std::distance(blocks.begin(), loop.base()) - 1);
            const size_t end = blocks[index].end_ip;
            blocks.erase(blocks.begin() + index + 1, blocks.end());
            if (line == "break") { blocks.erase(blocks.begin() + index); ip = end + 1; }
            else ip = end;
            continue;
        }

        if (line.rfind("return", 0) == 0 && (line.size() == 6 || std::isspace(static_cast<unsigned char>(line[6])))) {
            bool array = false;
            auto values = evaluateScriptReturnValues(line.substr(6), array, err);
            if (!err.empty()) break;
            ret_val = values.empty() ? 0.0 : values.front();
            if (returnValues) *returnValues = std::move(values);
            has_returned = true;
            break;
        }

        if (line.rfind("print(", 0) == 0 && line.back() == ')') {
            const auto text = formatPrintString(line.substr(6, line.size() - 7), err);
            if (!err.empty()) break;
            engineState().script_console_output.push_back(text);
            ++ip; continue;
        }

        if ((line.rfind("while(", 0) == 0 || line.rfind("while (", 0) == 0 || line.rfind("for(", 0) == 0 || line.rfind("for (", 0) == 0) && line.back() == '{') {
            const bool isFor = line.rfind("for", 0) == 0;
            const size_t open = line.find('(');
            const size_t close = findScriptHeaderClose(line, open);
            const size_t end = findMatchingBlockEnd(lines, ip);
            if (open == std::string::npos || close == std::string::npos || end == std::string::npos) { err = "Mismatched loop"; break; }
            auto existing = std::find_if(blocks.begin(), blocks.end(), [&](const FnBlock& b) { return b.start_ip == ip && (b.type == "for" || b.type == "while"); });
            const bool active = existing != blocks.end();
            std::string condition = line.substr(open + 1, close - open - 1), step;
            if (isFor) {
                const auto header = expression::splitArguments(condition, ';', 3);
                if (header.size() != 3) { err = "Invalid for loop"; break; }
                step = header[2];
                condition = header[1];
                if (!active && !executeScriptAssignments(header[0], err)) break;
            }
            const double value = condition.find_first_not_of(" \t") == std::string::npos ? 1.0 : evaluateScalar(condition, err, "Condition");
            if (!err.empty()) break;
            if (value != 0.0) {
                if (++loopIterations > 10000) { err = "Function loop iteration limit reached"; break; }
                if (!active) blocks.push_back({isFor ? "for" : "while", true, ip, end, condition, step});
                ++ip;
            } else {
                if (active) blocks.erase(existing);
                ip = end + 1;
            }
            continue;
        }

        if ((line.rfind("if (", 0) == 0 || line.rfind("if(", 0) == 0) && line.back() == '{') {
            size_t open_p = line.find('(');
            const size_t close_p = findScriptHeaderClose(line, open_p);
            size_t end_ip = findMatchingBlockEnd(lines, ip);
            if (open_p != std::string::npos && close_p != std::string::npos && end_ip != std::string::npos) {
                std::string cond_str = line.substr(open_p + 1, close_p - open_p - 1);
                double cv = evaluateScalar(cond_str, err, "Condition");
                if (!err.empty()) break;
                bool cond_ok = (!std::isnan(cv) && cv != 0.0);
                if (cond_ok) {
                    blocks.push_back({"if", true, ip, end_ip, ""});
                    ip++;
                } else {
                    blocks.push_back({"if", false, ip, end_ip, ""});
                    size_t next_b = findMatchingBlockEnd(lines, ip, true);
                    ip = (next_b != std::string::npos) ? next_b : end_ip;
                }
                continue;
            }
            err = "Mismatched if";
            break;
        } else if ((line.rfind("} else if", 0) == 0 || line.rfind("else if", 0) == 0 || line.rfind("} elif", 0) == 0 || line.rfind("elif", 0) == 0) && line.back() == '{') {
            if (blocks.empty() || blocks.back().type != "if") { err = "Unmatched else if"; break; }
            if (blocks.back().condition_met) { ip = blocks.back().end_ip; continue; }
            const size_t open = line.find('(');
            const size_t close = findScriptHeaderClose(line, open);
            if (open == std::string::npos || close == std::string::npos || close <= open) { err = "Invalid else if"; break; }
            const double value = evaluateScalar(line.substr(open + 1, close - open - 1), err, "Condition");
            if (!err.empty()) break;
            if (value != 0.0) { blocks.back().condition_met = true; ++ip; }
            else { const size_t next = findMatchingBlockEnd(lines, ip, true); ip = next == std::string::npos ? blocks.back().end_ip : next; }
            continue;
        } else if (line == "} else {" || line == "else {") {
            if (!blocks.empty() && blocks.back().type == "if") {
                if (blocks.back().condition_met) {
                    ip = blocks.back().end_ip;
                } else {
                    blocks.back().condition_met = true;
                    ip++;
                }
                continue;
            }
        } else if (isBlockEnd(line)) {
            if (!blocks.empty()) {
                auto& block = blocks.back();
                if (block.type == "for" || block.type == "while") {
                    if (block.type == "for" && block.step_str.find_first_not_of(" \t") != std::string::npos) {
                        if (!executeScriptAssignments(block.step_str, err)) break;
                    }
                    ip = block.start_ip;
                    continue;
                }
                blocks.pop_back();
            }
            ip++;
            continue;
        }

        bool isDef = false;
        evaluateInput(line, err, isDef);
        if (!err.empty()) break;
        ip++;
    }

    if (step_count >= max_steps && !has_returned && err.empty()) err = "Function statement limit reached";
    engineState().user_args = std::move(orig_args);
    engineState().user_arrays = std::move(orig_arrays);

    if (!err.empty()) return std::numeric_limits<double>::quiet_NaN();
    return ret_val;
}

void runScript(const std::string& code) {
    engineState().script_console_output.clear();
    if (writesHistoryVariable(code)) {
        engineState().script_console_output.push_back("History variables are read-only");
        return;
    }
    if (auto name = reservedAssignment(code); !name.empty()) {
        engineState().script_console_output.push_back("Reserved name: " + name);
        return;
    }
    engineState().plot_lines.clear();
    engineState().plot_hold = false;
    
    auto original_funcs = engineState().user_funcs;
    auto original_autocomplete = engineState().autocomplete_words;
    engineState().user_args.reserve(256);
    
    std::vector<size_t> sourceLines;
    std::vector<std::string> lines = splitIntoStatements(code, &sourceLines);
    
    size_t ip = 0;
    int max_steps = 1000000;
    int step_count = 0;
    int loop_iterations = 0;
    constexpr int max_loop_iterations = 10000;
    engineState().script_return_val = 0.0;
    engineState().script_has_returned = false;
    
    struct BlockState {
        std::string type;       // "if", "for", "while"
        bool condition_met;     // For "if" chain: has any branch been executed?
        size_t start_ip;        // For loops/conditionals: start of block header
        size_t end_ip;          // Ultimate end of the block (the matching end/endif/endfor/endwhile)
        
        std::string step_str;
    };
    std::vector<BlockState> blocks;
    
    while (ip < lines.size() && step_count < max_steps) {
        if (engineState().script_has_returned) break;
        step_count++;

        
        std::string line = lines[ip];
        line.erase(0, line.find_first_not_of(" \t\r"));
        line.erase(line.find_last_not_of(" \t\r") + 1);
        if (line.empty() || line[0] == '#' || line.rfind("//", 0) == 0) {
            ip++;
            continue;
        }
        if (line == "break" || line == "continue") {
            auto loop = std::find_if(blocks.rbegin(), blocks.rend(), [](const BlockState& block) { return block.type == "for" || block.type == "while"; });
            if (loop == blocks.rend()) { engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + line + " outside loop"); ++ip; continue; }
            const size_t index = static_cast<size_t>(std::distance(blocks.begin(), loop.base()) - 1);
            const size_t end = blocks[index].end_ip;
            blocks.erase(blocks.begin() + index + 1, blocks.end());
            if (line == "break") { blocks.erase(blocks.begin() + index); ip = end + 1; }
            else ip = end;
            continue;
        }
        
        if (line.rfind("return", 0) == 0 && (line.size() == 6 || line[6] == ' ' || line[6] == '\t')) {
            evaluateScriptReturn(line.substr(6), sourceLines[ip]);
            break;
        }

        if (handleScriptFunctionDeclaration(lines, sourceLines[ip], ip)) continue;

        // plot commands
        if (line.rfind("plot", 0) == 0) {
            std::string err;
            double result = 0.0;
            if (handlePlotCommands(line, err, result)) {
                if (!err.empty()) {
                    engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + err);
                }
                ip++;
                continue;
            }
        }

        // print(expr)
        if (line.rfind("print(", 0) == 0 && line.back() == ')') {
            std::string expr = line.substr(6, line.size() - 7);
            std::string err;
            std::string output = formatPrintString(expr, err);
            if (!err.empty()) {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + err);
            } else {
                engineState().script_console_output.push_back(output);
            }
            ip++;
        }

        // sleep(ms)
        else if (line.rfind("sleep(", 0) == 0 && line.back() == ')') {
            ip++;
        }
        // if (cond) {
        else if ((line.rfind("if (", 0) == 0 || line.rfind("if(", 0) == 0) && line.back() == '{') {
            const size_t open = line.find('(');
            const size_t close = findScriptHeaderClose(line, open);
            const size_t end = findMatchingBlockEnd(lines, ip);
            if (close == std::string::npos || end == std::string::npos) {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Mismatched if");
                ip = afterScriptBlock(lines, ip);
                continue;
            }
            std::string error;
            const bool condition = evaluateScriptCondition(line.substr(open + 1, close - open - 1), error);
            if (!error.empty()) {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + error);
                ip = end + 1;
                continue;
            }
            blocks.push_back({"if", condition, ip, end, ""});
            if (condition) {
                ++ip;
            } else {
                const size_t next = findMatchingBlockEnd(lines, ip, true);
                ip = next == std::string::npos ? end : next;
            }
        }
        // } else if (cond) { / else if (cond) { / } elif (cond) { / elif (cond) {
        else if ((line.rfind("} else if", 0) == 0 || line.rfind("else if", 0) == 0 ||
                  line.rfind("} elif", 0) == 0 || line.rfind("elif", 0) == 0) && line.back() == '{') {
            if (blocks.empty() || blocks.back().type != "if") {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Unmatched else if");
                ip = afterScriptBlock(lines, ip);
                continue;
            }
            if (blocks.back().condition_met) {
                ip = blocks.back().end_ip;
                continue;
            }
            const size_t open = line.find('(');
            const size_t close = findScriptHeaderClose(line, open);
            if (close == std::string::npos) {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Mismatched else if");
                ip = blocks.back().end_ip;
                continue;
            }
            std::string error;
            const bool condition = evaluateScriptCondition(line.substr(open + 1, close - open - 1), error);
            if (!error.empty()) {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + error);
                ip = blocks.back().end_ip;
                continue;
            }
            if (condition) {
                blocks.back().condition_met = true;
                ++ip;
            } else {
                const size_t next = findMatchingBlockEnd(lines, ip, true);
                ip = next == std::string::npos ? blocks.back().end_ip : next;
            }
        }
        // } else { / else {
        else if (line == "} else {" || line == "else {") {
            if (blocks.empty() || blocks.back().type != "if") {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Unmatched else");
                ip = afterScriptBlock(lines, ip);
                continue;
            }
            if (blocks.back().condition_met) {
                ip = blocks.back().end_ip;
            } else {
                blocks.back().condition_met = true;
                ++ip;
            }
        }
        // Both loop forms share block tracking, limits and exit handling.
        else if ((line.rfind("while(", 0) == 0 || line.rfind("while (", 0) == 0 ||
                  line.rfind("for(", 0) == 0 || line.rfind("for (", 0) == 0) && line.back() == '{') {
            const bool isFor = line.rfind("for", 0) == 0;
            const std::string type = isFor ? "for" : "while";
            const auto existing = std::find_if(blocks.begin(), blocks.end(), [ip](const BlockState& block) {
                return block.start_ip == ip;
            });
            const bool active = existing != blocks.end();
            const size_t open = line.find('(');
            const size_t close = findScriptHeaderClose(line, open);
            const size_t end = active ? existing->end_ip : findMatchingBlockEnd(lines, ip);
            if (close == std::string::npos || end == std::string::npos) {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Mismatched " + type);
                ip = afterScriptBlock(lines, ip);
                continue;
            }
            std::string conditionText(expression::trim(std::string_view(line).substr(open + 1, close - open - 1)));
            std::string step;
            std::string error;
            if (isFor) {
                const auto header = expression::splitArguments(conditionText, ';', 3);
                if (header.size() != 3) {
                    engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Mismatched for syntax");
                    ip = afterScriptBlock(lines, ip);
                    continue;
                }
                conditionText = header[1];
                step = header[2];
                if (!active) executeScriptAssignments(header[0], error);
            }
            const bool condition = error.empty() && ((isFor && conditionText.empty()) || evaluateScriptCondition(conditionText, error));
            if (!error.empty()) {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + error);
                if (active) blocks.erase(existing);
                ip = end + 1;
                continue;
            }
            if (!condition) {
                if (active) blocks.erase(existing);
                ip = end + 1;
                continue;
            }
            if (++loop_iterations > max_loop_iterations) {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Loop iteration limit reached");
                break;
            }
            if (!active) blocks.push_back({type, true, ip, end, step});
            ++ip;
        }
        else if (isBlockEnd(line)) {
            const auto block = std::find_if(blocks.rbegin(), blocks.rend(), [ip](const BlockState& candidate) {
                return candidate.end_ip == ip;
            });
            if (block == blocks.rend()) {
                ++ip;
            } else if (block->type == "if") {
                blocks.erase(std::prev(block.base()));
                ++ip;
            } else {
                std::string stepError;
                if (block->type == "for" && !executeScriptAssignments(block->step_str, stepError)) {
                    engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[block->start_ip]) + " Err: " + stepError);
                    break;
                }
                ip = block->start_ip;
            }
        }
        // Increment / Decrement: i++ or i--
        else if (line.size() > 2 && (line.substr(line.size() - 2) == "++" || line.substr(line.size() - 2) == "--" ||
                 line.substr(0, 2) == "++" || line.substr(0, 2) == "--")) {
            std::string error;
            if (!executeScriptAssignments(line, error)) {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + error);
            }
            ++ip;
        }
        // Array element assignment: A[idx] = expr
        else if (std::string name, idx_expr, val_expr; parseArrayElementAssignment(line, name, idx_expr, val_expr)) {
            std::string err;
            assignArrayElement(name, idx_expr, val_expr, err);
            if (!err.empty())
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + err);
            ++ip;
        }
        // standard assignment: name = expr or name op= expr
        else if (const size_t eq = expression::assignmentOffset(line, true); eq != std::string::npos) {
            std::string lhs = line.substr(0, eq);
            std::string rhs = line.substr(eq + 1);
            if (!lhs.empty() && (lhs.back() == '+' || lhs.back() == '-' || lhs.back() == '*' || lhs.back() == '/')) {
                char op = lhs.back();
                lhs = lhs.substr(0, lhs.size() - 1);
                rhs = lhs + " " + std::string(1, op) + " (" + rhs + ")";
            }
            lhs.erase(0, lhs.find_first_not_of(" \t"));
            lhs.erase(lhs.find_last_not_of(" \t") + 1);
            rhs.erase(0, rhs.find_first_not_of(" \t"));
            rhs.erase(rhs.find_last_not_of(" \t") + 1);
            
            // Check if LHS contains multiple variables separated by commas (tuple assignment e.g. x, y = fn())
            if (lhs.find(',') != std::string::npos) {
                std::string error;
                if (!assignScriptTuple(lhs, rhs, error)) {
                    engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + error);
                }
                if (engineState().script_has_returned) break;
                ip++;
                continue;
            }
            
            if (isReservedName(lhs)) { engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Reserved name: " + lhs); ip++; continue; }
            if (isScriptConstant(lhs)) {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Const Error");
                ip++;
                continue;
            }
            
            bool definition = false;
            std::string error;
            evaluateInput(lhs + "=" + rhs, error, definition);
            if (!error.empty()) {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + error);
            }
            if (engineState().script_has_returned) break;
            ip++;
            continue;
        }
        else {
            bool isDef = false;
            std::string eval_err;
            evaluateInput(line, eval_err, isDef);
            if (!eval_err.empty()) {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + eval_err);
            }
            ip++;
        }
    }
    
    if (step_count >= max_steps) {
        engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[std::min(ip, sourceLines.size() - 1)]) + " Err: Script statement limit reached");
    }
    
    auto& state = engineState();
    state.user_funcs = std::move(original_funcs);
    state.autocomplete_words = std::move(original_autocomplete);
    std::unordered_set<std::string_view> knownWords;
    const auto maxWords = state.autocomplete_words.size() + state.user_args.size() + state.user_arrays.size() + state.user_script_funcs.size();
    knownWords.reserve(maxWords);
    // Keep the keys valid while appending completion words.
    state.autocomplete_words.reserve(maxWords);
    knownWords.insert(state.autocomplete_words.begin(), state.autocomplete_words.end());
    for (const auto& arg : state.user_args) {
        if (knownWords.insert(arg.name).second) state.autocomplete_words.push_back(arg.name);
    }
    for (const auto& array : state.user_arrays) {
        if (knownWords.insert(array.first).second) state.autocomplete_words.push_back(array.first);
    }
    for (const auto& function : state.user_script_funcs) {
        const auto completion = function.name + "(";
        if (knownWords.find(completion) == knownWords.end()) {
            state.autocomplete_words.push_back(completion);
            knownWords.insert(state.autocomplete_words.back());
        }
    }
    // Note: user_arrays intentionally not restored — scripts may define arrays
    // that affect subsequent plot rendering (e.g., x, y1) or global state.
}
