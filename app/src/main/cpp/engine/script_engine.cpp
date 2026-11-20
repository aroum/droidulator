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
extern bool parseBinaryOp(const std::string& rhs, std::string& op1, std::string& op, std::string& op2);
extern std::vector<double> evaluateArrayBinaryOp(const std::string& op1_str, const std::string& op_str, const std::string& op2_str, bool& is_scalar_result, double& scalar_val, std::string& err);
extern std::vector<double> parseArrayExpr(const std::string& rhs, bool& is_array, std::string& err);
extern std::string preprocessLen(const std::string& s);
extern std::string preprocessVectorStats(const std::string& s);


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
    const double value = evaluate(expression, error);
    return error.empty() && !std::isnan(value) && value != 0.0;
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
    if (open == std::string::npos || close == std::string::npos) return false;

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
    if (!expression::trim(parameterText).empty() && params.empty()) error = "Invalid parameter list";
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

static void evaluateScriptReturn(std::string expressionText, size_t sourceLine) {
    auto& state = engineState();
    expressionText.erase(0, expressionText.find_first_not_of(" \t"));
    expressionText.erase(expressionText.find_last_not_of(" \t") + 1);
    if (expressionText.empty()) expressionText = "0";

    const auto parts = expression::splitArguments(expressionText);
    std::string error;
    bool isArray = false;
    std::vector<double> values;
    if (parts.size() > 1) {
        isArray = true;
        values.reserve(parts.size());
        for (auto part : parts) {
            part.erase(0, part.find_first_not_of(" \t"));
            part.erase(part.find_last_not_of(" \t") + 1);
            const auto expanded = preprocessArrayLookups(part, error);
            if (!error.empty()) break;
            const double value = evaluate(expanded, error);
            if (!error.empty()) break;
            values.push_back(value);
        }
    } else {
        values = parseArrayExpr(expressionText, isArray, error);
        if (!isArray || !error.empty()) {
            isArray = false;
            error.clear();
            const auto expanded = preprocessArrayLookups(expressionText, error);
            if (error.empty()) values = {evaluate(expanded, error)};
        }
    }

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

static bool executeScriptAssignments(const std::string& step, std::string& err) {
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
            size_t open_paren = line.find('(');
            const size_t close_paren = open_paren == std::string::npos ? std::string::npos : expression::matchingGroupEnd(line, open_paren);
            if (open_paren != std::string::npos && close_paren != std::string::npos && close_paren > open_paren) {
                std::string cond_str = line.substr(open_paren + 1, close_paren - open_paren - 1);
                cond_str.erase(0, cond_str.find_first_not_of(" \t"));
                cond_str.erase(cond_str.find_last_not_of(" \t") + 1);
                
                size_t end_ip = findMatchingBlockEnd(lines, ip);
                if (end_ip != std::string::npos) {
                    std::string err;
                    const bool cond_bool = evaluateScriptCondition(cond_str, err);

                    
                    if (!err.empty()) {
                        engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + err);
                        ip = end_ip + 1;
                    } else if (cond_bool) {
                        blocks.push_back({ "if", true, ip, end_ip, "" });
                        ip++;
                    } else {
                        blocks.push_back({ "if", false, ip, end_ip, "" });
                        size_t next_branch = findMatchingBlockEnd(lines, ip, true);
                        if (next_branch != std::string::npos) {
                            ip = next_branch;
                        } else {
                            ip = end_ip;
                        }
                    }
                } else {
                    engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Mismatched if");
                    ip++;
                }
            } else {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Mismatched if");
                ip++;
            }
        }
        // } else if (cond) { / else if (cond) { / } elif (cond) { / elif (cond) {
        else if ((line.rfind("} else if", 0) == 0 || line.rfind("else if", 0) == 0 ||
                  line.rfind("} elif", 0) == 0 || line.rfind("elif", 0) == 0) && line.back() == '{') {
            if (!blocks.empty() && blocks.back().type == "if") {
                if (blocks.back().condition_met) {
                    ip = blocks.back().end_ip;
                } else {
                    size_t open_paren = line.find('(');
                    const size_t close_paren = open_paren == std::string::npos ? std::string::npos : expression::matchingGroupEnd(line, open_paren);
                    if (open_paren != std::string::npos && close_paren != std::string::npos && close_paren > open_paren) {
                        std::string cond_str = line.substr(open_paren + 1, close_paren - open_paren - 1);
                        cond_str.erase(0, cond_str.find_first_not_of(" \t"));
                        cond_str.erase(cond_str.find_last_not_of(" \t") + 1);
                        
                        std::string err;
                        const bool cond_bool = evaluateScriptCondition(cond_str, err);
                        
                        if (!err.empty()) {
                            engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + err);
                            ip = blocks.back().end_ip;
                        } else if (cond_bool) {
                            blocks.back().condition_met = true;
                            ip++;
                        } else {
                            size_t next_branch = findMatchingBlockEnd(lines, ip, true);
                            if (next_branch != std::string::npos) {
                                ip = next_branch;
                            } else {
                                ip = blocks.back().end_ip;
                            }
                        }
                    } else {
                        engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Mismatched else if");
                        ip++;
                    }
                }
            } else {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Unmatched else if");
                ip++;
            }
        }
        // } else { / else {
        else if (line == "} else {" || line == "else {") {
            if (!blocks.empty() && blocks.back().type == "if") {
                if (blocks.back().condition_met) {
                    ip = blocks.back().end_ip;
                } else {
                    blocks.back().condition_met = true;
                    ip++;
                }
            } else {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Unmatched else");
                ip++;
            }
        }
        // while (cond) {
        else if ((line.rfind("while (", 0) == 0 || line.rfind("while(", 0) == 0) && line.back() == '{') {
            bool found_block = false;
            size_t block_idx = 0;
            for (size_t b = 0; b < blocks.size(); ++b) {
                if (blocks[b].start_ip == ip) {
                    found_block = true;
                    block_idx = b;
                    break;
                }
            }
            
            size_t open_paren = line.find('(');
            const size_t close_paren = open_paren == std::string::npos ? std::string::npos : expression::matchingGroupEnd(line, open_paren);
            if (open_paren != std::string::npos && close_paren != std::string::npos && close_paren > open_paren) {
                std::string cond_str = line.substr(open_paren + 1, close_paren - open_paren - 1);
                cond_str.erase(0, cond_str.find_first_not_of(" \t"));
                cond_str.erase(cond_str.find_last_not_of(" \t") + 1);
                
                size_t end_ip = found_block ? blocks[block_idx].end_ip : findMatchingBlockEnd(lines, ip);
                if (end_ip != std::string::npos) {
                    std::string err;
                    const bool cond_bool = evaluateScriptCondition(cond_str, err);
                    
                    if (!err.empty()) {
                        engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + err);
                        if (found_block) blocks.erase(blocks.begin() + block_idx);
                        ip = end_ip + 1;
                    } else if (cond_bool) {
                        if (++loop_iterations > max_loop_iterations) { engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Loop iteration limit reached"); break; }
                        if (!found_block) {
                            blocks.push_back({ "while", true, ip, end_ip, "" });
                        }
                        ip++;
                    } else {
                        if (found_block) {
                            blocks.erase(blocks.begin() + block_idx);
                        }
                        ip = end_ip;
                    }
                } else {
                    engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Mismatched while");
                    ip++;
                }
            } else {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Mismatched while");
                ip++;
            }
        }
        // for (init; cond; step) {
        else if ((line.rfind("for (", 0) == 0 || line.rfind("for(", 0) == 0) && line.back() == '{') {
            bool found_block = false;
            size_t block_idx = 0;
            for (size_t b = 0; b < blocks.size(); ++b) {
                if (blocks[b].start_ip == ip) {
                    found_block = true;
                    block_idx = b;
                    break;
                }
            }

            size_t open_paren = line.find('(');
            const size_t close_paren = open_paren == std::string::npos ? std::string::npos : expression::matchingGroupEnd(line, open_paren);
            if (open_paren != std::string::npos && close_paren != std::string::npos && close_paren > open_paren) {
                std::string inner = line.substr(open_paren + 1, close_paren - open_paren - 1);
                size_t first_semi = std::string::npos;
                size_t second_semi = std::string::npos;
                std::vector<char> headerGroups;
                for (const auto& token : expression::tokenize(inner)) {
                    if (token.kind == expression::Kind::Open) headerGroups.push_back(token.text.front());
                    else if (token.kind == expression::Kind::Close) {
                        if (!expression::closeGroup(headerGroups, token.text.front())) break;
                    } else if (headerGroups.empty() && token.text == ";") {
                        if (first_semi == std::string::npos) first_semi = token.start;
                        else { second_semi = token.start; break; }
                    }
                }
                if (first_semi != std::string::npos && second_semi != std::string::npos) {
                    std::string init_str = inner.substr(0, first_semi);
                    std::string cond_str = inner.substr(first_semi + 1, second_semi - first_semi - 1);
                    std::string step_str = inner.substr(second_semi + 1);
                    
                    auto trim = [](std::string& s) {
                        s.erase(0, s.find_first_not_of(" \t\r"));
                        s.erase(s.find_last_not_of(" \t\r") + 1);
                    };
                    trim(init_str);
                    trim(cond_str);
                    trim(step_str);
                    size_t end_ip = found_block ? blocks[block_idx].end_ip : findMatchingBlockEnd(lines, ip);
                    if (end_ip != std::string::npos) {
                        std::string err;
                        if (!found_block && !init_str.empty()) {
                            executeScriptAssignments(init_str, err);
                        }

                        const bool cond_bool = err.empty() && evaluateScriptCondition(cond_str, err);
                        if (!err.empty()) {
                            engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + err);
                            if (found_block) blocks.erase(blocks.begin() + block_idx);
                            ip = end_ip + 1;
                        } else if (cond_bool) {
                            if (++loop_iterations > max_loop_iterations) { engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Loop iteration limit reached"); break; }
                            if (!found_block) {
                                blocks.push_back({ "for", true, ip, end_ip, step_str });
                            }
                            ip++;
                        } else {
                            if (found_block) {
                                blocks.erase(blocks.begin() + block_idx);
                            }
                            ip = end_ip + 1;
                        }
                    } else {
                        engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Mismatched for");
                        ip++;
                    }
                } else {
                    engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Mismatched for syntax");
                    ip++;
                }
            } else {
                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Mismatched for");
                ip++;
            }
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
                const auto lhs_vars = expression::splitArguments(lhs);
                if (lhs_vars.empty() || std::any_of(lhs_vars.begin(), lhs_vars.end(),
                    [](const auto& name) { return !expression::isIdentifier(name); })) {
                    engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Invalid tuple assignment");
                    ++ip;
                    continue;
                }
                if (std::unordered_set<std::string>(lhs_vars.begin(), lhs_vars.end()).size() != lhs_vars.size()) {
                    engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Duplicate tuple target");
                    ++ip;
                    continue;
                }
                bool reserved = false;
                for (const auto& name : lhs_vars) {
                    if (isReservedName(name) || std::any_of(engineState().user_consts.begin(), engineState().user_consts.end(), [&](const UserConst& c) { return c.name == name; })) { reserved = true; break; }
                }
                if (reserved) { engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: Const or reserved name"); ip++; continue; }
                
                // Evaluate RHS expression
                bool is_arr = false;
                std::string parse_err;
                std::vector<double> vals;
                
                // Use the same function executor as expression calls.
                size_t open_p = rhs.find('(');
                bool executed_fn = false;
                if (open_p != std::string::npos && rhs.back() == ')') {
                    std::string fn_name = rhs.substr(0, open_p);
                    fn_name.erase(0, fn_name.find_first_not_of(" \t"));
                    fn_name.erase(fn_name.find_last_not_of(" \t") + 1);
                    const auto found = std::find_if(engineState().user_script_funcs.begin(), engineState().user_script_funcs.end(), [&](const CustomScriptFunc& f) { return f.name == fn_name; });
                    if (found != engineState().user_script_funcs.end()) {
                        executed_fn = true;
                        const auto argText = rhs.substr(open_p + 1, rhs.size() - open_p - 2);
                        auto argExprs = argText.find_first_not_of(" \t") == std::string::npos ? std::vector<std::string>{} : expression::splitArguments(argText);
                        std::vector<double> evaluated;
                        for (const auto& arg : argExprs) {
                            evaluated.push_back(evaluate(arg, parse_err));
                            if (!parse_err.empty()) break;
                        }
                        if (parse_err.empty()) executeScriptFunc(fn_name, evaluated, parse_err, &vals);
                    }
                }
                if (!parse_err.empty()) { engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + parse_err); ip++; continue; }

                if (!executed_fn) {
                    vals = parseArrayExpr(rhs, is_arr, parse_err);
                    if (!is_arr || !parse_err.empty()) {
                        std::string eval_err;
                        const auto expanded = preprocessArrayLookups(preprocessLen(rhs), eval_err);
                        double single_v = eval_err.empty() ? evaluate(expanded, eval_err) : 0.0;
                        if (!eval_err.empty()) { engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + eval_err); ip++; continue; }
                        vals = {single_v};
                    }
                }
                
                for (size_t var_i = 0; var_i < lhs_vars.size(); ++var_i) {
                    const std::string& var_name = lhs_vars[var_i];
                    double val = (var_i < vals.size()) ? vals[var_i] : 0.0;
                    engineState().user_arrays.erase(var_name);
                    bool found = false;
                    for (auto& a : engineState().user_args) {
                        if (a.name == var_name) {
                            a.val = val;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        engineState().user_args.push_back({var_name, val});
                    }
                    if (std::find(engineState().autocomplete_words.begin(), engineState().autocomplete_words.end(), var_name) == engineState().autocomplete_words.end()) {
                        engineState().autocomplete_words.push_back(var_name);
                    }
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
            
            std::string op1, op, op2;
            std::string err;
            bool is_arr_op = false;
            if (parseBinaryOp(rhs, op1, op, op2)) {
                auto trim = [](std::string& s) {
                    s.erase(0, s.find_first_not_of(" \t"));
                    s.erase(s.find_last_not_of(" \t") + 1);
                };
                trim(op1); trim(op2);
                bool is1_arr = false, is2_arr = false;
                std::string dummy_err;
                std::vector<double> a1 = parseArrayExpr(op1, is1_arr, dummy_err);
                std::vector<double> a2 = parseArrayExpr(op2, is2_arr, dummy_err);
                bool op1_is_scalar = false, op2_is_scalar = false;
                for (const auto& ua : engineState().user_args) {
                    if (ua.name == op1) op1_is_scalar = true;
                    if (ua.name == op2) op2_is_scalar = true;
                }
                if (op == ".*" || op == "./" || (is1_arr && !a1.empty()) || (is2_arr && !a2.empty()) || (!op1_is_scalar && engineState().user_arrays.find(op1) != engineState().user_arrays.end() && !engineState().user_arrays[op1].empty()) || (!op2_is_scalar && engineState().user_arrays.find(op2) != engineState().user_arrays.end() && !engineState().user_arrays[op2].empty())) {
                    is_arr_op = true;
                }
            }

            if (is_arr_op) {
                bool is_scalar = false;
                double scalar_res = 0;
                std::vector<double> res = evaluateArrayBinaryOp(op1, op, op2, is_scalar, scalar_res, err);
                if (!err.empty()) {
                    engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + err);
                } else if (is_scalar) {
                    engineState().user_arrays.erase(lhs);
                    bool found = false;
                    for (auto& a : engineState().user_args) {
                        if (a.name == lhs) {
                            a.val = scalar_res;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        engineState().user_args.push_back({lhs, scalar_res});
                    }
                    if (std::find(engineState().autocomplete_words.begin(), engineState().autocomplete_words.end(), lhs) == engineState().autocomplete_words.end()) {
                        engineState().autocomplete_words.push_back(lhs);
                    }
                } else {
                    engineState().user_arrays[lhs] = res;
                }
            } else {
                bool is_arr = false;
                std::string parse_arr_err;
                std::vector<double> arr = parseArrayExpr(rhs, is_arr, parse_arr_err);
                if (is_arr && parse_arr_err.empty()) {
                    engineState().user_arrays[lhs] = arr;
                    engineState().user_args.erase(std::remove_if(engineState().user_args.begin(), engineState().user_args.end(), [&](const UserArg& a) {
                        return a.name == lhs;
                    }), engineState().user_args.end());
                } else {
                    engineState().user_arrays.erase(lhs);
                    // Pre-expand len() so that expressions like len(Y) become scalars
                    // before we scan rhs for array variable references.
                    std::string len_err;
                    std::string rhs_prepped = preprocessVectorStats(preprocessArrayLookups(preprocessLen(rhs), len_err));
                    // Check if rhs references a user array by name (e.g., "sin(x)" where x is array).
                    // If so, evaluate the expression element-wise substituting each array value.
                    std::string source_arr_name;
                    const std::vector<double>* source_arr = nullptr;
                    for (const auto& kv : engineState().user_arrays) {
                        if (kv.second.empty()) continue;
                        const std::string& aname = kv.first;
                        // Skip if aname is also defined as a scalar variable in user_args
                        bool is_scalar_var = false;
                        for (const auto& arg : engineState().user_args) {
                            if (arg.name == aname) { is_scalar_var = true; break; }
                        }
                        if (is_scalar_var) continue;

                        size_t pos = rhs_prepped.find(aname);
                        while (pos != std::string::npos) {
                            bool pre_ok = (pos == 0 || (!std::isalnum((unsigned char)rhs_prepped[pos-1]) && rhs_prepped[pos-1] != '_'));
                            size_t end = pos + aname.size();
                            bool post_ok = (end >= rhs_prepped.size() || (!std::isalnum((unsigned char)rhs_prepped[end]) && rhs_prepped[end] != '_'));
                            bool not_indexed = (end >= rhs_prepped.size() || rhs_prepped[end] != '[');
                            if (pre_ok && post_ok && not_indexed) {
                                source_arr_name = aname;
                                source_arr = &kv.second;
                                break;
                            }
                            pos = rhs_prepped.find(aname, pos + 1);
                        }
                        if (source_arr) break;
                    }

                    if (source_arr && !source_arr->empty()) {
                        const auto sourceValues = *source_arr;
                        bool name_is_id = (!source_arr_name.empty() && (std::isalpha((unsigned char)source_arr_name[0]) || source_arr_name[0] == '_'));
                        if (name_is_id) {
                            for (char c : source_arr_name) {
                                if (!std::isalnum((unsigned char)c) && c != '_') { name_is_id = false; break; }
                            }
                        }
                        if (name_is_id) {
                            int x_idx = -1;
                            for (size_t k = 0; k < engineState().user_args.size(); ++k) {
                                if (engineState().user_args[k].name == source_arr_name) { x_idx = (int)k; break; }
                            }
                            bool created_var = false;
                            double old_val = 0.0;
                            if (x_idx != -1) {
                                old_val = engineState().user_args[x_idx].val;
                            } else {
                                engineState().user_args.push_back({source_arr_name, 0.0});
                                x_idx = (int)engineState().user_args.size() - 1;
                                created_var = true;
                            }
                            std::string prep_err;
                            std::string prep_rhs = rhs_prepped;
                            std::vector<double> elem_res;
                            elem_res.reserve(sourceValues.size());
                            bool all_ok = true;
                            std::string eval_err;
                            for (double val : sourceValues) {
                                auto current = std::find_if(engineState().user_args.begin(), engineState().user_args.end(), [&](const UserArg& a) { return a.name == source_arr_name; });
                                if (current == engineState().user_args.end()) engineState().user_args.push_back({source_arr_name, val});
                                else current->val = val;
                                eval_err.clear();
                                double y = evaluate(prep_rhs, eval_err);
                                if (!eval_err.empty()) { all_ok = false; break; }
                                elem_res.push_back(y);
                            }
                            auto current = std::find_if(engineState().user_args.begin(), engineState().user_args.end(), [&](const UserArg& a) { return a.name == source_arr_name; });
                            if (created_var) { if (current != engineState().user_args.end()) engineState().user_args.erase(current); }
                            else if (current != engineState().user_args.end()) current->val = old_val;
                            else engineState().user_args.push_back({source_arr_name, old_val});

                            if (!all_ok) {
                                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + eval_err);
                            } else {
                                engineState().user_arrays[lhs] = elem_res;
                                engineState().user_args.erase(std::remove_if(engineState().user_args.begin(), engineState().user_args.end(), [&](const UserArg& a) {
                                    return a.name == lhs;
                                }), engineState().user_args.end());
                            }
                        } else {
                            std::string eval_err;
                            const auto expanded = preprocessArrayLookups(preprocessLen(rhs), eval_err);
                            double val = eval_err.empty() ? evaluate(expanded, eval_err) : 0.0;
                            if (!eval_err.empty()) {
                                engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + eval_err);
                            } else {
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
                                if (std::find(engineState().autocomplete_words.begin(), engineState().autocomplete_words.end(), lhs) == engineState().autocomplete_words.end()) {
                                    engineState().autocomplete_words.push_back(lhs);
                                }
                            }
                        }
                    } else {
                        std::string eval_err;
                        std::string prep_rhs = preprocessArrayLookups(preprocessLen(rhs), eval_err);
                        double val = evaluate(prep_rhs, eval_err);
                        if (!eval_err.empty()) {
                            engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + eval_err);
                        } else {
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
                            if (std::find(engineState().autocomplete_words.begin(), engineState().autocomplete_words.end(), lhs) == engineState().autocomplete_words.end()) {
                                engineState().autocomplete_words.push_back(lhs);
                            }
                        }
                    }
                }
            }
            if (engineState().script_has_returned) break;
            ip++;
            continue;
        }
        else {
            if (line.rfind("plot", 0) == 0) {
                std::string err;
                double result = 0.0;
                if (handlePlotCommands(line, err, result)) {
                    if (!err.empty()) {
                        engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + err);
                    }
                }
            } else {
                bool isDef = false;
                std::string eval_err;
                evaluateInput(line, eval_err, isDef);
                if (!eval_err.empty()) engineState().script_console_output.push_back("L:" + std::to_string(sourceLines[ip]) + " Err: " + eval_err);
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
