#pragma once

#include "expression_tokens.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>

namespace expression {

// Nodes are stored in evaluation order. Child indices always point backwards,
// so evaluating an arbitrarily long operator chain does not recurse.
class ArithmeticExpression {
public:
    static constexpr size_t maxArguments = 24;
    using Arguments = std::array<double, maxArguments>;
    using VariadicFunction = double (*)(const Arguments&);
    using VariadicLookup = VariadicFunction (*)(std::string_view);
    using UnaryFunction = double (*)(double);
    using BinaryFunction = double (*)(double, double);
    using BinaryLookup = BinaryFunction (*)(std::string_view);
    using UnaryLookup = UnaryFunction (*)(std::string_view);
    using ArrayLookup = const std::vector<double>* (*)(std::string_view);
    using ScalarLookup = std::optional<double> (*)(std::string_view);
    using CustomFunction = std::function<double(const std::vector<double>&, std::string&)>;
    using CustomLookup = CustomFunction (*)(std::string_view);

    static std::optional<double> evaluate(std::string_view source, std::string& error,
                                          ScalarLookup lookup = nullptr, UnaryLookup unary = nullptr,
                                          ArrayLookup arrays = nullptr, BinaryLookup binary = nullptr,
                                          VariadicLookup variadic = nullptr, CustomLookup custom = nullptr) {
        ArithmeticExpression parser(tokenize(source));
        const auto root = parser.parse(0, 0);
        if (root == invalid || parser.next != parser.tokens.size()) return std::nullopt;
        // Resolve all names before evaluating any operations. Unsupported
        // expressions must fall back without changing the caller's error state.
        for (auto& node : parser.nodes) {
            if (node.op == '%') {
                if (!binary || !(node.binaryFunction = binary("mod"))) return std::nullopt;
                continue;
            }
            if (node.symbol.empty()) continue;
            if (node.op == 'l') {
                if (!arrays || (lookup && lookup(node.symbol))) return std::nullopt;
                const auto array = arrays(node.symbol);
                if (!array) return std::nullopt;
                continue;
            }
            if (node.op == '[') {
                if (!arrays) return std::nullopt;
                continue;
            }
            if (node.op == 'c') {
                const auto& arguments = parser.callArguments[node.right];
                const auto arity = arguments.size();
                // Four-argument reductions can bind their first argument as a loop index.
                if (arity == 4 && (parser.nodes[arguments.front()].op == 0 || parser.nodes[arguments.front()].op == 'v') &&
                    !parser.nodes[arguments.front()].symbol.empty() &&
                    (node.symbol == "sum" || node.symbol == "prod" ||
                     node.symbol == "min" || node.symbol == "max")) return std::nullopt;
                if (arity == 1 && unary && (node.function = unary(node.symbol))) continue;
                if (arity == 2 && binary && (node.binaryFunction = binary(node.symbol))) continue;
                if (variadic && (node.variadicFunction = variadic(node.symbol))) continue;
                if (custom && (node.customFunction = custom(node.symbol))) continue;
                return std::nullopt;
            }
            if (node.op == 'f') {
                if (!unary || !(node.function = unary(node.symbol))) return std::nullopt;
                continue;
            }
            if (!lookup) return std::nullopt;
            const auto value = lookup(node.symbol);
            if (!value) return std::nullopt;
            node.op = 'v';
        }
        for (auto& node : parser.nodes) {
            if (!node.op) continue;
            if (node.op == 'v') {
                const auto value = lookup(node.symbol);
                if (!value) {
                    error = "Unknown variable: " + std::string(node.symbol);
                    return std::numeric_limits<double>::quiet_NaN();
                }
                node.value = *value;
                continue;
            }
            if (node.op == 'l') {
                const auto array = arrays(node.symbol);
                if (!array) {
                    error = "Array " + std::string(node.symbol) + " not defined";
                    return std::numeric_limits<double>::quiet_NaN();
                }
                node.value = static_cast<double>(array->size());
                continue;
            }
            const double left = parser.nodes[node.left].value;
            const double right = node.op == 'c' ? 0 : parser.nodes[node.right].value;
            if (node.op >= 'A' && node.op <= 'I') {
                if (!std::isfinite(left) || !std::isfinite(right)) {
                    error = std::isnan(left) || std::isnan(right) ? "Math Error" : "Overflow Error";
                    return std::numeric_limits<double>::quiet_NaN();
                }
            }
            switch (node.op) {
                case 'A': node.value = left != 0 || right != 0; break;
                case 'B': node.value = (left != 0) != (right != 0); break;
                case 'C': node.value = left != 0 && right != 0; break;
                case 'D': node.value = left == right; break;
                case 'E': node.value = left != right; break;
                case 'F': node.value = left <= right; break;
                case 'G': node.value = left >= right; break;
                case 'H': node.value = left < right; break;
                case 'I': node.value = left > right; break;
                case '[': {
                    // Calls may restore the array table; resolve storage only when used.
                    const auto array = arrays(node.symbol);
                    if (!array) {
                        error = "Array " + std::string(node.symbol) + " not defined";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    if (!std::isfinite(left) || std::trunc(left) != left || left < 1 ||
                        left > static_cast<double>(array->size())) {
                        error = "Invalid or out of bounds index for array " + std::string(node.symbol) +
                            " (size " + std::to_string(array->size()) + ")";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    node.value = (*array)[static_cast<size_t>(left) - 1];
                    break;
                }
                case 'c': {
                    const auto& args = parser.callArguments[node.right];
                    if (node.customFunction) {
                        std::vector<double> values;
                        values.reserve(args.size());
                        for (const auto argument : args) {
                            const double value = parser.nodes[argument].value;
                            if (!std::isfinite(value)) {
                                error = std::isnan(value) ? "Math Error" : "Overflow Error";
                                return std::numeric_limits<double>::quiet_NaN();
                            }
                            values.push_back(value);
                        }
                        node.value = node.customFunction(values, error);
                        if (!error.empty()) return std::numeric_limits<double>::quiet_NaN();
                    } else if (node.function) {
                        node.value = node.function(parser.nodes[args[0]].value);
                    } else if (node.binaryFunction) {
                        node.value = node.binaryFunction(parser.nodes[args[0]].value, parser.nodes[args[1]].value);
                    } else {
                        Arguments values;
                        values.fill(std::numeric_limits<double>::quiet_NaN());
                        for (size_t i = 0; i < args.size(); ++i) values[i] = parser.nodes[args[i]].value;
                        node.value = node.variadicFunction(values);
                    }
                    break;
                }
                case 'f': node.value = node.function(left); break;
                case '+': node.value = left + right; break;
                case '-': node.value = left - right; break;
                case '*': node.value = left * right; break;
                case 'n':
                    if (!std::isfinite(left)) {
                        error = std::isnan(left) ? "Math Error" : "Overflow Error";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    node.value = left == 0.0;
                    break;
                case '%': node.value = node.binaryFunction(left, right); break;
                case '^': node.value = std::pow(left, right); break;
                case '/':
                    if (right == 0) {
                        error = "Division by Zero";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    node.value = left / right;
                    break;
            }
        }
        const double value = parser.nodes[root].value;
        if (std::isinf(value)) error = "Overflow Error";
        else if (std::isnan(value)) error = "Math Error";
        return value;
    }

private:
    struct Node {
        double value = 0;
        char op = 0;
        size_t left = 0;
        size_t right = 0;
        std::string_view symbol{};
        UnaryFunction function = nullptr;
        BinaryFunction binaryFunction = nullptr;
        VariadicFunction variadicFunction = nullptr;
        CustomFunction customFunction;
    };

    explicit ArithmeticExpression(std::vector<Token> input) : tokens(std::move(input)) {}

    static constexpr size_t invalid = std::string_view::npos;
    std::vector<Token> tokens;
    std::vector<Node> nodes;
    std::vector<std::vector<size_t>> callArguments;
    size_t next = 0;

    size_t add(Node node) {
        nodes.push_back(node);
        return nodes.size() - 1;
    }

    struct Operator {
        char opcode;
        int precedence;
        bool rightAssociative = false;
    };

    static Operator binaryOperator(std::string_view op) {
        // Preserve the legacy relational evaluator's order and grouping.
        if (op == "||" || op == "or") return {'A', 1, true};
        if (op == "^^" || op == "xor") return {'B', 2, true};
        if (op == "&&" || op == "and") return {'C', 3, true};
        if (op == "==") return {'D', 4, true};
        if (op == "!=") return {'E', 5, true};
        if (op == "<=") return {'F', 6, true};
        if (op == ">=") return {'G', 7, true};
        if (op == "<") return {'H', 8, true};
        if (op == ">") return {'I', 9, true};
        if (op == "+" || op == "-") return {op.front(), 10};
        if (op == "*" || op == "/" || op == "%") return {op.front(), 11};
        if (op == "^") return {'^', 12};
        return {0, -1};
    }

    size_t parse(int minimum, unsigned depth) {
        if (depth >= 128 || next == tokens.size()) return invalid;
        std::vector<char> prefixes;
        while (next < tokens.size()) {
            const auto prefix = tokens[next].text;
            if (prefix != "+" && prefix != "-" && prefix != "!" && prefix != "not") break;
            if (prefix != "+") prefixes.push_back(prefix == "-" ? '-' : 'n');
            ++next;
        }
        if (next == tokens.size()) return invalid;
        const auto token = tokens[next++];
        size_t left;
        if (token.kind == Kind::Number || token.kind == Kind::ScaledNumber) {
            std::string literal(token.text);
            if (token.kind == Kind::ScaledNumber) {
                const auto split = token.text.find_first_not_of("0123456789.");
                const auto suffix = token.text.substr(split, token.text.substr(split, 2) == "da" ? 2 : 1);
                const auto fraction = token.text.substr(split + suffix.size());
                literal.resize(split);
                if (!fraction.empty()) {
                    literal += '.';
                    literal += fraction;
                }
                literal += 'e';
                literal += siExponent(suffix);
            }
            char* end = nullptr;
            const double value = std::strtod(literal.c_str(), &end);
            if (end != literal.c_str() + literal.size() || !std::isfinite(value)) return invalid;
            left = add({value});
        } else {
            if (token.kind == Kind::Word) {
                if (token.text == "len" && next + 2 < tokens.size() && tokens[next].text == "(" &&
                    tokens[next + 1].kind == Kind::Word && tokens[next + 2].text == ")") {
                    left = add({0, 'l', 0, 0, tokens[next + 1].text});
                    next += 3;
                } else if (next < tokens.size() && (tokens[next].text == "(" || tokens[next].text == "[")) {
                    const bool index = tokens[next++].text == "[";
                    if (!index && next < tokens.size() && tokens[next].text == ")") {
                        ++next;
                        left = add({0, 'c', 0, callArguments.size(), token.text});
                        callArguments.emplace_back();
                        // Continue parsing operators and postfix factorial below.
                    } else {
                        const auto argument = parse(0, depth + 1);
                        if (argument == invalid) return invalid;
                        if (index) {
                            left = add({0, '[', argument, 0, token.text});
                        } else {
                            std::vector<size_t> args{argument};
                            while (next < tokens.size() && tokens[next].kind == Kind::Comma) {
                                if (args.size() == maxArguments) return invalid;
                                ++next;
                                const auto arg = parse(0, depth + 1);
                                if (arg == invalid) return invalid;
                                args.push_back(arg);
                            }
                            left = add({0, 'c', argument, callArguments.size(), token.text});
                            callArguments.push_back(std::move(args));
                        }
                        if (next == tokens.size() || tokens[next++].text != (index ? "]" : ")")) return invalid;
                    }
                } else {
                    left = add({0, 0, 0, 0, token.text});
                }
            } else if (token.text == "(") {
                left = parse(0, depth + 1);
                if (left == invalid || next == tokens.size() || tokens[next++].text != ")") return invalid;
            } else {
                return invalid;
            }
        }
        while (next < tokens.size() && tokens[next].text == "!") {
            ++next;
            left = add({0, 'f', left, 0, "fact"});
        }
        for (auto prefix = prefixes.rbegin(); prefix != prefixes.rend(); ++prefix) {
            if (*prefix == 'n') left = add({0, 'n', left});
            else {
                const auto minusOne = add({-1});
                left = add({0, '*', minusOne, left});
            }
        }
        while (next < tokens.size()) {
            const auto op = binaryOperator(tokens[next].text);
            if (op.precedence < minimum) break;
            ++next;
            if (op.opcode == '%' && (next == tokens.size() ||
                (tokens[next].kind != Kind::Number && tokens[next].kind != Kind::ScaledNumber &&
                 tokens[next].kind != Kind::Word && tokens[next].text != "(" && tokens[next].text != "["))) {
                // A sign after % starts a new term following a postfix percentage.
                return invalid;
            }
            const auto right = parse(op.precedence + (op.rightAssociative ? 0 : 1), depth + 1);
            if (right == invalid) return invalid;
            left = add({0, op.opcode, left, right});
        }
        return left;
    }
};
}
