#pragma once

#include "expression_tokens.h"
#include "vector_statistics.h"
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
    struct CustomFunction {
        size_t arity = 0;
        std::function<double(const std::vector<double>&, std::string&)> invoke;
        std::function<double(const std::vector<double>&, std::string&, std::optional<std::vector<double>>&)> invokeTyped;
        explicit operator bool() const { return static_cast<bool>(invoke) || static_cast<bool>(invokeTyped); }
        double operator()(const std::vector<double>& values, std::string& error,
                          std::optional<std::vector<double>>* array = nullptr) const {
            if (array) array->reset();
            if (values.size() != arity) {
                error = "Arg Count Mismatch";
                return std::numeric_limits<double>::quiet_NaN();
            }
            std::optional<std::vector<double>> returnedArray;
            const double result = invokeTyped ? invokeTyped(values, error, returnedArray) : invoke(values, error);
            if (error.empty()) {
                if (returnedArray) {
                    if (array) *array = std::move(returnedArray);
                    else {
                        error = "Function result must be scalar";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                }
            }
            return result;
        }
    };
    using CustomLookup = CustomFunction (*)(std::string_view);

    static std::optional<double> evaluate(std::string_view source, std::string& error,
                                          ScalarLookup lookup = nullptr, UnaryLookup unary = nullptr,
                                          ArrayLookup arrays = nullptr, BinaryLookup binary = nullptr,
                                          VariadicLookup variadic = nullptr, CustomLookup custom = nullptr,
                                          std::optional<std::vector<double>>* arrayResult = nullptr) {
        if (arrayResult) arrayResult->reset();
        ArithmeticExpression parser(tokenize(source));
        if (!arrayResult && !parser.tokens.empty() && parser.tokens.front().text == "[" &&
            matchingGroupEnd(source, parser.tokens.front().start) == parser.tokens.back().start)
            return std::nullopt; // Array-valued roots still use the existing array evaluator.
        const auto root = parser.parse(0, 0);
        if (root == invalid || parser.next != parser.tokens.size()) return std::nullopt;
        const auto isArrayNode = [&](size_t index) {
            const auto& argument = parser.nodes[index];
            if (argument.scalarResult) return false;
            return argument.op == 'w' || argument.op == 'b' || argument.op == 't' || argument.op == 'u' || argument.op == 'r' || argument.op == 'a' || argument.arrayConditional || (arrays && !argument.op && !argument.symbol.empty() &&
                !(lookup && lookup(argument.symbol)) && arrays(argument.symbol));
        };
        // Bind array consumers before scalar name lookup.
        for (auto& node : parser.nodes) {
            if (node.op == '+' || node.op == '-' || node.op == '*' || node.op == '/' || node.op == 'J' || node.op == 'K') {
                const bool leftArray = isArrayNode(node.left);
                const bool rightArray = isArrayNode(node.right);
                if (leftArray || rightArray) {
                    if (leftArray && rightArray && node.op == '*' &&
                        !parser.nodes[node.left].dynamicType && !parser.nodes[node.right].dynamicType) node.op = 'd';
                    else {
                        node.arrayOperation = node.op;
                        node.op = 't';
                        node.arraySlot = parser.literalArrays.size();
                        parser.literalArrays.emplace_back();
                    }
                    node.dynamicType = parser.nodes[node.left].dynamicType || parser.nodes[node.right].dynamicType;
                    if (leftArray && !parser.nodes[node.left].op) parser.nodes[node.left].op = 'a';
                    if (rightArray && !parser.nodes[node.right].op) parser.nodes[node.right].op = 'a';
                }
                continue;
            }
            if (node.op == 'i') {
                if (!isArrayNode(node.left)) return std::nullopt;
                if (!parser.nodes[node.left].op) parser.nodes[node.left].op = 'a';
                continue;
            }
            if (node.op != 'c') continue;
            const auto& args = parser.callArguments[node.right];
            if (node.symbol == "if" || node.symbol == "iff") {
                if ((node.symbol == "if" && args.size() != 3) || args.size() < 3 || args.size() % 2 == 0)
                    return std::nullopt;
                bool allArrays = isArrayNode(args.back());
                bool anyArrays = allArrays;
                for (size_t i = 0; i + 1 < args.size(); i += 2) {
                    if (isArrayNode(args[i]) && !parser.nodes[args[i]].dynamicType) return std::nullopt;
                    const bool branchArray = isArrayNode(args[i + 1]);
                    allArrays = allArrays && branchArray;
                    anyArrays = anyArrays || branchArray;
                }
                node.op = 'q';
                node.arrayConditional = anyArrays;
                node.dynamicType = anyArrays && !allArrays;
                for (size_t i = 1; i < args.size(); i += 2)
                    node.dynamicType = node.dynamicType || parser.nodes[args[i]].dynamicType;
                node.dynamicType = node.dynamicType || parser.nodes[args.back()].dynamicType;
                if (anyArrays) {
                    node.arraySlot = parser.literalArrays.size();
                    parser.literalArrays.emplace_back();
                    for (size_t i = 1; i < args.size(); i += 2)
                        if (isArrayNode(args[i]) && !parser.nodes[args[i]].op) parser.nodes[args[i]].op = 'a';
                    if (isArrayNode(args.back()) && !parser.nodes[args.back()].op) parser.nodes[args.back()].op = 'a';
                }
                continue;
            }
            if (custom) {
                node.customFunction = custom(node.symbol);
                if (node.customFunction && node.customFunction.arity != args.size()) {
                    node.op = 'e';
                    continue;
                }
            }
            if (node.symbol == "dot" && args.size() == 2 && binary && binary("dot") &&
                isArrayNode(args[0]) && isArrayNode(args[1])) {
                node.op = 'd';
                node.left = args[0];
                node.right = args[1];
                if (!parser.nodes[node.left].op) parser.nodes[node.left].op = 'a';
                if (!parser.nodes[node.right].op) parser.nodes[node.right].op = 'a';
                continue;
            }
            if (node.symbol == "len" && args.size() == 1 && isArrayNode(args.front())) {
                node.op = 'z';
                node.left = args.front();
                if (!parser.nodes[node.left].op) parser.nodes[node.left].op = 'a';
                continue;
            }
            if (args.size() != 1 || !isArrayNode(args.front())) {
                const bool builtin = (args.size() == 1 && unary && unary(node.symbol)) ||
                    (args.size() == 2 && binary && binary(node.symbol)) || (variadic && variadic(node.symbol));
                if (builtin) node.customFunction = {};
                if (node.customFunction && node.customFunction.invokeTyped) {
                    node.op = 'w';
                    node.dynamicType = true;
                    node.arraySlot = parser.literalArrays.size();
                    parser.literalArrays.emplace_back();
                }
                continue;
            }
            const auto statistic = vectorStatistic(node.symbol);
            if (statistic && variadic && variadic(node.symbol)) {
                node.op = 's';
                node.statistic = *statistic;
            } else {
                if (unary) node.function = unary(node.symbol);
                if (node.function) node.customFunction = {};
                if (!node.function && !node.customFunction) continue;
                if (node.customFunction && node.customFunction.arity != 1) return std::nullopt;
                node.op = 'u';
                node.dynamicType = parser.nodes[args.front()].dynamicType;
                node.arraySlot = parser.literalArrays.size();
                parser.literalArrays.emplace_back();
            }
            node.left = args.front();
            if (!parser.nodes[node.left].op) parser.nodes[node.left].op = 'a';
        }
        const auto visitScalarInputs = [&](const Node& node, const auto& visit) {
            if (node.op == 'e' || node.op == 'c' || node.op == 'w' || node.op == 'b' || node.op == 'r') {
                for (size_t argument : parser.callArguments[node.right]) visit(argument);
            } else if (node.op == 'q') {
                const auto& args = parser.callArguments[node.right];
                for (size_t i = 0; i + 1 < args.size(); i += 2) visit(args[i]);
            } else if (node.op == 'n' || node.op == 'f' || node.op == '[') {
                visit(node.left);
            } else if (node.op == '^' || node.op == '%' || (node.op >= 'A' && node.op <= 'I')) {
                visit(node.left);
                visit(node.right);
            }
            if (node.op == 'i') visit(node.right);
        };
        // Reject consumers that cannot yet preserve a dynamically selected type
        // before any branch or user callback executes.
        std::vector<bool> typedConsumers(parser.nodes.size(), false);
        for (const auto& node : parser.nodes) {
            visitScalarInputs(node, [&](size_t argument) {
                if (parser.nodes[argument].dynamicType) typedConsumers[argument] = true;
            });
            if (node.op == 's' || node.op == 'z' || node.op == 'u' || node.op == 'i' || node.op == 'd')
                typedConsumers[node.left] = true;
            if (node.op == 'd') typedConsumers[node.right] = true;
            if (node.op == 't') {
                typedConsumers[node.left] = true;
                typedConsumers[node.right] = true;
            }
            if (node.op != 'q') continue;
            const auto& args = parser.callArguments[node.right];
            for (size_t i = 1; i < args.size(); i += 2) typedConsumers[args[i]] = true;
            typedConsumers[args.back()] = true;
        }
        for (size_t i = 0; i < parser.nodes.size(); ++i)
            if (parser.nodes[i].dynamicType && i != root && !typedConsumers[i]) return std::nullopt;
        std::vector<bool> consumedArrays(parser.nodes.size(), false);
        for (const auto& node : parser.nodes) {
            visitScalarInputs(node, [&](size_t argument) {
                if (parser.nodes[argument].dynamicType) consumedArrays[argument] = true;
            });
            if (node.arrayConditional) {
                const auto& args = parser.callArguments[node.right];
                for (size_t i = 1; i < args.size(); i += 2) consumedArrays[args[i]] = true;
                consumedArrays[args.back()] = true;
            }
            if (node.op == 's' || node.op == 'z' || node.op == 'd' || node.op == 'i' || node.op == 't' || node.op == 'u') consumedArrays[node.left] = true;
            if (node.op == 'd' || node.op == 't') consumedArrays[node.right] = true;
        }
        const bool rootArray = isArrayNode(root);
        if (rootArray) {
            if (!arrayResult && !parser.nodes[root].dynamicType) return std::nullopt;
            if (!parser.nodes[root].op) parser.nodes[root].op = 'a';
            consumedArrays[root] = true;
        }
        for (size_t i = 0; i < parser.nodes.size(); ++i)
            if ((parser.nodes[i].op == 'w' || parser.nodes[i].op == 'a' || parser.nodes[i].op == 'b' || parser.nodes[i].op == 't' || parser.nodes[i].op == 'u' || parser.nodes[i].op == 'r' || parser.nodes[i].arrayConditional) && !consumedArrays[i]) return std::nullopt;
        const auto arrayValue = [&](size_t index) -> const std::vector<double>* {
            const auto& node = parser.nodes[index];
            if (node.op == 'b') return &parser.literalArrays[node.left];
            if (node.op == 'w' || node.op == 't' || node.op == 'u' || node.op == 'r' || node.arrayConditional) return &parser.literalArrays[node.arraySlot];
            return arrays ? arrays(node.symbol) : nullptr;
        };
        // Resolve all names before evaluating any operations. Unsupported
        // expressions must fall back without changing the caller's error state.
        for (auto& node : parser.nodes) {
            if (node.op == 'w' || node.op == 'e' || node.op == 'a' || node.op == 'b' || node.op == 's' || node.op == 'd' || node.op == 'z' || node.op == 'i' || node.op == 't' || node.op == 'u' || node.op == 'r' || node.op == 'q') continue;
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
                if (custom && (node.customFunction = custom(node.symbol))) {
                    if (node.customFunction.arity != arity) node.op = 'e';
                    continue;
                }
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
        // Evaluate only dependencies requested by the root. A conditional requests
        // conditions in order and exactly one branch; inactive nodes never execute.
        struct Frame { size_t index; size_t stage = 0; };
        std::vector<Frame> work;
        work.reserve(parser.nodes.size());
        work.push_back({root});
        std::vector<bool> complete(parser.nodes.size(), false);
        while (!work.empty()) {
            auto& frame = work.back();
            const size_t index = frame.index;
            auto& node = parser.nodes[index];
            if (complete[index]) { work.pop_back(); continue; }
            if (node.op == 'e') {
                error = "Arg Count Mismatch";
                return std::numeric_limits<double>::quiet_NaN();
            }
            if (node.op == 'q') {
                const auto& args = parser.callArguments[node.right];
                size_t selected = args.back();
                bool waiting = false;
                while (frame.stage + 1 < args.size()) {
                    const size_t condition = args[frame.stage];
                    if (!complete[condition]) {
                        work.push_back({condition});
                        waiting = true;
                        break;
                    }
                    if (isArrayNode(condition)) {
                        error = "Condition must be scalar";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    const double value = parser.nodes[condition].value;
                    if (!std::isfinite(value)) {
                        error = std::isnan(value) ? "Math Error" : "Overflow Error";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    if (std::abs(value) > 1e-12) {
                        selected = args[frame.stage + 1];
                        break;
                    }
                    frame.stage += 2;
                }
                if (waiting) continue;
                if (!complete[selected]) {
                    work.push_back({selected});
                    continue;
                }
                if (node.dynamicType && !isArrayNode(selected)) node.arrayConditional = false;
                if (node.arrayConditional) {
                    const auto& source = parser.nodes[selected];
                    const auto values = arrayValue(selected);
                    if (!values) {
                        error = "Array not defined";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    auto& target = parser.literalArrays[node.arraySlot];
                    if (source.op == 'b') target = std::move(parser.literalArrays[source.left]);
                    else if (source.op == 'w' || source.op == 't' || source.op == 'u' || source.op == 'r' || source.arrayConditional)
                        target = std::move(parser.literalArrays[source.arraySlot]);
                    else target = *values;
                }
                node.value = parser.nodes[selected].value;
                complete[index] = true;
                work.pop_back();
                continue;
            }
            if (frame.stage == 0) {
                frame.stage = 1;
                if (node.op == 'c' || node.op == 'w' || node.op == 'b' || node.op == 'r') {
                    const auto& args = parser.callArguments[node.right];
                    for (auto argument = args.rbegin(); argument != args.rend(); ++argument)
                        if (!complete[*argument]) work.push_back({*argument});
                } else if (node.op == 'f' || node.op == 'n' || node.op == '[' || node.op == 's' || node.op == 'z' || node.op == 'u') {
                    if (!complete[node.left]) work.push_back({node.left});
                } else if (node.op && node.op != 'v' && node.op != 'l' && node.op != 'a') {
                    if (!complete[node.right]) work.push_back({node.right});
                    if (!complete[node.left]) work.push_back({node.left});
                }
                // Re-enter after the dependencies, including leaf nodes.
                continue;
            }
            bool invalidScalar = false;
            visitScalarInputs(node, [&](size_t argument) {
                invalidScalar = invalidScalar || isArrayNode(argument);
            });
            if (invalidScalar) {
                error = "Expression argument must be scalar";
                return std::numeric_limits<double>::quiet_NaN();
            }
            complete[index] = true;
            work.pop_back();
            if (!node.op || node.op == 'a') continue;
            if (node.op == 'w') {
                std::vector<double> arguments;
                for (size_t argument : parser.callArguments[node.right]) {
                    const double value = parser.nodes[argument].value;
                    if (!std::isfinite(value)) {
                        error = std::isnan(value) ? "Math Error" : "Overflow Error";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    arguments.push_back(value);
                }
                std::optional<std::vector<double>> returnedArray;
                node.value = node.customFunction(arguments, error, &returnedArray);
                if (!error.empty()) return std::numeric_limits<double>::quiet_NaN();
                if (returnedArray) parser.literalArrays[node.arraySlot] = std::move(*returnedArray);
                else node.scalarResult = true;
                continue;
            }
            if (node.op == 'r') {
                const auto& args = parser.callArguments[node.right];
                parser.literalArrays[node.arraySlot] = vectorRange(parser.nodes[args[0]].value,
                    parser.nodes[args[1]].value, parser.nodes[args[2]].value, 100000, error);
                if (!error.empty()) return std::numeric_limits<double>::quiet_NaN();
                continue;
            }
            if (node.op == 'u') {
                if (!isArrayNode(node.left)) {
                    const double input = parser.nodes[node.left].value;
                    node.value = node.function ? node.function(input) : node.customFunction({input}, error);
                    if (!error.empty()) return std::numeric_limits<double>::quiet_NaN();
                    if (!std::isfinite(node.value)) {
                        error = std::isnan(node.value) ? "Math Error" : "Overflow Error";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    node.scalarResult = true;
                    continue;
                }
                const auto* source = arrayValue(node.left);
                if (!source) {
                    error = "Array not defined";
                    return std::numeric_limits<double>::quiet_NaN();
                }
                // Script calls can restore the session array table while mapping.
                std::vector<double> snapshot;
                if (node.customFunction) { snapshot = *source; source = &snapshot; }
                auto& values = parser.literalArrays[node.arraySlot];
                values.reserve(source->size());
                std::vector<double> argument(1);
                for (double input : *source) {
                    argument[0] = input;
                    const double value = node.function ? node.function(input) : node.customFunction(argument, error);
                    if (!error.empty()) return std::numeric_limits<double>::quiet_NaN();
                    if (!std::isfinite(value)) {
                        error = std::isnan(value) ? "Math Error" : "Overflow Error";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    values.push_back(value);
                }
                continue;
            }
            if (node.op == 't') {
                const auto left = isArrayNode(node.left) ? arrayValue(node.left) : nullptr;
                const auto right = isArrayNode(node.right) ? arrayValue(node.right) : nullptr;
                if ((isArrayNode(node.left) && !left) || (isArrayNode(node.right) && !right)) {
                    error = "Array not defined";
                    return std::numeric_limits<double>::quiet_NaN();
                }
                const char operation = node.arrayOperation;
                if (!left && !right) {
                    const double a = parser.nodes[node.left].value;
                    const double b = parser.nodes[node.right].value;
                    if ((operation == '/' || operation == 'K') && b == 0.0) {
                        error = "Division by Zero";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    node.value = operation == '+' ? a + b : operation == '-' ? a - b :
                        (operation == '*' || operation == 'J') ? a * b : a / b;
                    node.scalarResult = true;
                    continue;
                }
                if (operation == '*' && left && right) {
                    node.value = vectorDot(*left, *right, error);
                    if (!error.empty()) return std::numeric_limits<double>::quiet_NaN();
                    node.scalarResult = true;
                    continue;
                }
                const auto text = operation == 'J' ? std::string(".*") : operation == 'K' ? std::string("./") : std::string(1, operation);
                parser.literalArrays[node.arraySlot] = vectorElementwise(text, left, parser.nodes[node.left].value,
                    right, parser.nodes[node.right].value, error);
                if (!error.empty()) return std::numeric_limits<double>::quiet_NaN();
                continue;
            }
            if (node.op == 'i') {
                const auto& source = parser.nodes[node.left];
                if (!isArrayNode(node.left)) {
                    error = "Indexing requires an array";
                    return std::numeric_limits<double>::quiet_NaN();
                }
                node.value = indexedValue(arrayValue(node.left), parser.nodes[node.right].value,
                    source.symbol.empty() ? "expression" : source.symbol, error);
                if (!error.empty()) return std::numeric_limits<double>::quiet_NaN();
                continue;
            }
            if (node.op == 'b') {
                auto& values = parser.literalArrays[node.left];
                values.reserve(parser.callArguments[node.right].size());
                for (const auto argument : parser.callArguments[node.right]) {
                    const double value = parser.nodes[argument].value;
                    if (!std::isfinite(value)) {
                        error = std::isnan(value) ? "Math Error" : "Overflow Error";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    values.push_back(value);
                }
                continue;
            }
            if (node.op == 'd') {
                if (!isArrayNode(node.left) || !isArrayNode(node.right)) {
                    error = "Dot product requires arrays";
                    return std::numeric_limits<double>::quiet_NaN();
                }
                const auto left = arrayValue(node.left);
                const auto right = arrayValue(node.right);
                if (!left || !right) {
                    error = "Array not defined";
                    return std::numeric_limits<double>::quiet_NaN();
                }
                node.value = vectorDot(*left, *right, error);
                if (!error.empty()) return std::numeric_limits<double>::quiet_NaN();
                continue;
            }
            if (node.op == 's' || node.op == 'z') {
                if (!isArrayNode(node.left)) {
                    if (node.op == 'z') {
                        error = "Length requires an array";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    node.value = evaluateStatistic(node.statistic, {parser.nodes[node.left].value});
                    continue;
                }
                const auto name = parser.nodes[node.left].symbol;
                const auto array = arrayValue(node.left);
                if (!array) {
                    error = "Array " + std::string(name) + " not defined";
                    return std::numeric_limits<double>::quiet_NaN();
                }
                node.value = node.op == 'z' ? static_cast<double>(array->size()) : evaluateStatistic(node.statistic, *array);
                continue;
            }
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
                case '[':
                    node.value = indexedValue(arrays(node.symbol), left, node.symbol, error);
                    if (!error.empty()) return std::numeric_limits<double>::quiet_NaN();
                    break;
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
                case 'J':
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
                case 'K':
                case '/':
                    if (right == 0) {
                        error = "Division by Zero";
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    node.value = left / right;
                    break;
            }
        }
        if (rootArray && isArrayNode(root)) {
            if (!arrayResult) {
                error = "Expression result must be scalar";
                return std::numeric_limits<double>::quiet_NaN();
            }
            const auto values = arrayValue(root);
            if (!values) {
                error = "Array not defined";
                return std::numeric_limits<double>::quiet_NaN();
            }
            const double first = values->empty() ? 0.0 : values->front();
            const auto& node = parser.nodes[root];
            if (node.op == 'b') *arrayResult = std::move(parser.literalArrays[node.left]);
            else if (node.op == 'w' || node.op == 't' || node.op == 'u' || node.op == 'r' || node.arrayConditional) *arrayResult = std::move(parser.literalArrays[node.arraySlot]);
            else *arrayResult = *values;
            return first;
        }
        const double value = parser.nodes[root].value;
        if (std::isinf(value)) error = "Overflow Error";
        else if (std::isnan(value)) error = "Math Error";
        return value;
    }

private:
    static double indexedValue(const std::vector<double>* array, double index, std::string_view name, std::string& error) {
        if (!array) {
            error = "Array " + std::string(name) + " not defined";
        } else if (!std::isfinite(index) || std::trunc(index) != index || index < 1 ||
                   index > static_cast<double>(array->size())) {
            error = "Invalid or out of bounds index for array " + std::string(name) +
                " (size " + std::to_string(array->size()) + ")";
        } else {
            return (*array)[static_cast<size_t>(index) - 1];
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

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
        VectorStatistic statistic = VectorStatistic::Sum;
        char arrayOperation = 0;
        size_t arraySlot = 0;
        bool arrayConditional = false;
        bool dynamicType = false;
        bool scalarResult = false;
        bool grouped = false;
    };

    explicit ArithmeticExpression(std::vector<Token> input) : tokens(std::move(input)) {}

    static constexpr size_t invalid = std::string_view::npos;
    std::vector<Token> tokens;
    std::vector<Node> nodes;
    std::vector<std::vector<size_t>> callArguments;
    std::vector<std::vector<double>> literalArrays;
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
        if (op == ".*") return {'J', 11};
        if (op == "./") return {'K', 11};
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
        if (token.text == "[") {
            std::vector<size_t> elements;
            if (next < tokens.size() && tokens[next].text != "]") {
                while (true) {
                    if (elements.size() >= 100000) return invalid;
                    const auto element = parse(0, depth + 1);
                    if (element == invalid) return invalid;
                    elements.push_back(element);
                    if (next == tokens.size() || tokens[next].kind != Kind::Comma) break;
                    ++next;
                }
            }
            if (next == tokens.size() || tokens[next++].text != "]") return invalid;
            if (elements.size() == 1 && nodes[elements.front()].op == 'r') left = elements.front();
            else {
                left = add({0, 'b', literalArrays.size(), callArguments.size()});
                literalArrays.emplace_back();
                callArguments.push_back(std::move(elements));
            }
        } else if (token.kind == Kind::Number || token.kind == Kind::ScaledNumber) {
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
                nodes[left].grouped = true;
            } else {
                return invalid;
            }
        }
        while (next < tokens.size() && tokens[next].text == "[") {
            ++next;
            const auto index = parse(0, depth + 1);
            if (index == invalid || next == tokens.size() || tokens[next++].text != "]") return invalid;
            left = add({0, 'i', left, index});
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
            const bool postfixPercent = tokens[next].text == "%" && (next + 1 == tokens.size() ||
                (tokens[next + 1].kind != Kind::Number && tokens[next + 1].kind != Kind::ScaledNumber &&
                 tokens[next + 1].kind != Kind::Word && tokens[next + 1].text != "(" && tokens[next + 1].text != "["));
            if (postfixPercent) {
                // A percentage includes the current additive term. Defer it to
                // that parser frame so 200+10% uses 200 as its relative base,
                // while 2*50% scales the complete product by one hundredth.
                if (minimum > 10) break;
                ++next;
                const auto expression = nodes[left];
                const bool relative = !expression.grouped && (expression.op == '+' || expression.op == '-');
                if (relative) {
                    // Replace the unfinished additive root rather than leaving an
                    // unused array consumer in the typed dependency graph.
                    if (left + 1 != nodes.size()) return invalid;
                    nodes.pop_back();
                }
                const auto hundredth = add({0.01});
                if (relative) {
                    const auto adjustment = add({0, '*', expression.left, expression.right});
                    const auto percentage = add({0, '*', adjustment, hundredth});
                    left = add({0, expression.op, expression.left, percentage});
                } else left = add({0, '*', left, hundredth});
                nodes[left].grouped = true;
                continue;
            }
            const auto op = binaryOperator(tokens[next].text);
            if (op.precedence < minimum) break;
            ++next;
            const auto right = parse(op.precedence + (op.rightAssociative ? 0 : 1), depth + 1);
            if (right == invalid) return invalid;
            left = add({0, op.opcode, left, right});
        }
        if (minimum == 0 && next < tokens.size() && tokens[next].text == ":") {
            ++next;
            const auto bound = parse(1, depth + 1);
            if (bound == invalid) return invalid;
            size_t step;
            size_t end;
            if (next < tokens.size() && tokens[next].text == ":") {
                ++next;
                step = bound;
                end = parse(1, depth + 1);
                if (end == invalid) return invalid;
            } else {
                step = add({1.0});
                end = bound;
            }
            Node range{0, 'r', 0, callArguments.size()};
            range.arraySlot = literalArrays.size();
            literalArrays.emplace_back();
            callArguments.push_back({left, step, end});
            left = add(std::move(range));
        }
        return left;
    }
};
}
