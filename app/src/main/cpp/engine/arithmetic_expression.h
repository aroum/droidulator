#pragma once

#include "expression_tokens.h"
#include <cmath>
#include <cstdlib>
#include <limits>

namespace expression {

// Nodes are stored in evaluation order. Child indices always point backwards,
// so evaluating an arbitrarily long operator chain does not recurse.
class ArithmeticExpression {
public:
    using UnaryFunction = double (*)(double);
    using UnaryLookup = UnaryFunction (*)(std::string_view);
    using ArrayLookup = const std::vector<double>* (*)(std::string_view);
    using ScalarLookup = std::optional<double> (*)(std::string_view);

    static std::optional<double> evaluate(std::string_view source, std::string& error,
                                          ScalarLookup lookup = nullptr, UnaryLookup unary = nullptr,
                                          ArrayLookup arrays = nullptr) {
        ArithmeticExpression parser(tokenize(source));
        const auto root = parser.parse(0, 0);
        if (root == invalid || parser.next != parser.tokens.size()) return std::nullopt;
        // Resolve all names before evaluating any operations. Unsupported
        // expressions must fall back without changing the caller's error state.
        for (auto& node : parser.nodes) {
            if (node.symbol.empty()) continue;
            if (node.op == '[') {
                if (!arrays) return std::nullopt;
                node.array = arrays(node.symbol);
                continue;
            }
            if (node.op == 'f') {
                if (!unary || !(node.function = unary(node.symbol))) return std::nullopt;
                continue;
            }
            if (!lookup) return std::nullopt;
            const auto value = lookup(node.symbol);
            if (!value) return std::nullopt;
            node.value = *value;
        }
        for (auto& node : parser.nodes) {
            if (!node.op) continue;
            const double left = parser.nodes[node.left].value;
            const double right = parser.nodes[node.right].value;
            switch (node.op) {
                case '[':
                    if (!node.array) {
                        error = "Array " + std::string(node.symbol) + " not defined";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    if (!std::isfinite(left) || std::trunc(left) != left || left < 1 ||
                        left > static_cast<double>(node.array->size())) {
                        error = "Invalid or out of bounds index for array " + std::string(node.symbol) +
                            " (size " + std::to_string(node.array->size()) + ")";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    node.value = (*node.array)[static_cast<size_t>(left) - 1];
                    break;
                case 'f': node.value = node.function(left); break;
                case '+': node.value = left + right; break;
                case '-': node.value = left - right; break;
                case '*': node.value = left * right; break;
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
        const std::vector<double>* array = nullptr;
    };

    explicit ArithmeticExpression(std::vector<Token> input) : tokens(std::move(input)) {}

    static constexpr size_t invalid = std::string_view::npos;
    std::vector<Token> tokens;
    std::vector<Node> nodes;
    size_t next = 0;

    size_t add(Node node) {
        nodes.push_back(node);
        return nodes.size() - 1;
    }

    static int precedence(std::string_view op) {
        if (op == "+" || op == "-") return 1;
        if (op == "*" || op == "/") return 2;
        if (op == "^") return 3;
        return -1;
    }

    size_t parse(int minimum, unsigned depth) {
        if (depth >= 128 || next == tokens.size()) return invalid;
        bool negative = false;
        while (next < tokens.size() && (tokens[next].text == "+" || tokens[next].text == "-")) {
            negative ^= tokens[next++].text == "-";
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
                if (next < tokens.size() && (tokens[next].text == "(" || tokens[next].text == "[")) {
                    const bool index = tokens[next++].text == "[";
                    const auto argument = parse(0, depth + 1);
                    if (argument == invalid || next == tokens.size() || tokens[next++].text != (index ? "]" : ")")) return invalid;
                    left = add({0, index ? '[' : 'f', argument, 0, token.text});
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
        if (negative) {
            const auto minusOne = add({-1});
            left = add({0, '*', minusOne, left});
        }
        while (next < tokens.size()) {
            const auto op = tokens[next].text;
            const int priority = precedence(op);
            if (priority < minimum) break;
            ++next;
            // Preserve the existing engine's left-associative exponentiation.
            const auto right = parse(priority + 1, depth + 1);
            if (right == invalid) return invalid;
            left = add({0, op.front(), left, right});
        }
        return left;
    }
};
}
