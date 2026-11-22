#include "arithmetic_expression.h"
#include <charconv>
#include <tuple>
#include "math_engine.h"
#include "script_engine.h"
#include "tinyexpr.h"
#include <sstream>
#include <iomanip>

std::vector<double> parseArrayExpr(const std::string& rhs, bool& is_array, std::string& err);

static thread_local size_t loopEvaluationDepth = 0;
static thread_local size_t loopIterationsRemaining = 100000;
// Formula parameters and reduction indices are local to the active evaluation.
static thread_local std::vector<std::vector<UserArg>> scalarBindings;
static thread_local std::vector<std::map<std::string, std::vector<double>>> arrayBindings;

class ScalarBindingScope {
public:
    explicit ScalarBindingScope(std::vector<UserArg> values, std::map<std::string, std::vector<double>> arrays = {}) {
        scalarBindings.push_back(std::move(values));
        arrayBindings.push_back(std::move(arrays));
    }
    ~ScalarBindingScope() { arrayBindings.pop_back(); scalarBindings.pop_back(); }
    ScalarBindingScope(const ScalarBindingScope&) = delete;
    ScalarBindingScope& operator=(const ScalarBindingScope&) = delete;
};

static const std::vector<double>* scopedArray(std::string_view name) {
    if (arrayBindings.empty()) return nullptr;
    const auto found = arrayBindings.back().find(std::string(name));
    return found == arrayBindings.back().end() ? nullptr : &found->second;
}

static std::optional<double> scopedScalar(std::string_view name) {
    if (!scalarBindings.empty()) {
        for (const auto& argument : scalarBindings.back())
            if (argument.name == name) return argument.val;
    }
    return std::nullopt;
}

static std::optional<UserFunc> parseUserFunctionDefinition(std::string_view source) {
    const size_t equals = expression::assignmentOffset(source);
    if (equals == std::string_view::npos) return std::nullopt;
    const auto lhs = expression::trim(source.substr(0, equals));
    const auto rhs = expression::trim(source.substr(equals + 1));
    const size_t open = lhs.find('(');
    const size_t close = open == std::string_view::npos ? std::string_view::npos : expression::matchingGroupEnd(lhs, open);
    if (close == std::string_view::npos) return std::nullopt;
    return UserFunc{
        std::string(expression::trim(lhs.substr(0, open))),
        expression::splitArguments(lhs.substr(open + 1, close - open - 1)),
        std::string(rhs)
    };
}

std::vector<UserFunc> getParsedUserFuncs() {
    std::vector<UserFunc> result;
    result.reserve(engineState().user_funcs.size());
    for (const auto& definition : engineState().user_funcs) {
        if (auto parsed = parseUserFunctionDefinition(definition)) result.push_back(std::move(*parsed));
    }
    return result;
}

inline bool checkParentheses(const std::string& expr, std::string& err) {
    std::vector<char> stack;
    for (const auto& token : expression::tokenize(expr)) {
        if (token.kind == expression::Kind::Open) stack.push_back(token.text.front());
        else if (token.kind == expression::Kind::Close &&
                 !expression::closeGroup(stack, token.text.front())) {
            err = "Brackets Error";
            return false;
        }
    }
    if (!stack.empty()) {
        err = "Brackets Error";
        return false;
    }
    return true;
}

static bool isValidLoopIdent(const std::string& name) {
    return name != "pi" && name != "e" && name != "phi" && expression::isIdentifier(name);
}

static bool isDefinedScalarName(const std::string& s) {
    if (scopedScalar(s)) return true;
    for (const auto& ua : engineState().user_args) if (ua.name == s) return true;
    for (const auto& uc : engineState().user_consts) if (uc.name == s) return true;
    return false;
}

static bool exprReferencesIdent(const std::string& expr, const std::string& ident) {
    const auto tokens = expression::tokenize(expr);
    return std::any_of(tokens.begin(), tokens.end(), [&](const auto& token) {
        return token.kind == expression::Kind::Word && token.text == ident;
    });
}

static std::optional<double> evaluateAst(const std::string& source, std::string& error,
    expression::ArithmeticExpression::PreparedExpression* prepared = nullptr);

static double evaluateIndexedReduction(std::string_view name, const std::string& index,
        double start, double end, const std::string& body, std::string& err) {
    const double int64Limit = std::ldexp(1.0, 63);
    if (!std::isfinite(start) || !std::isfinite(end) || start < -int64Limit || start >= int64Limit || end < -int64Limit || end >= int64Limit) {
        err = "Overflow Error: invalid loop bounds"; return std::numeric_limits<double>::quiet_NaN();
    }
    int64_t i_start = static_cast<int64_t>(std::llround(start));
    int64_t i_end = static_cast<int64_t>(std::llround(end));
    const long double count = i_end < i_start ? 0 : static_cast<long double>(i_end) - static_cast<long double>(i_start) + 1;
    if (count > loopIterationsRemaining) {
        err = "Overflow Error: loop limit exceeded";
        return std::numeric_limits<double>::quiet_NaN();
    }
    auto arguments = scalarBindings.empty() ? std::vector<UserArg>{} : scalarBindings.back();
    size_t var_idx = 0;
    while (var_idx < arguments.size() && arguments[var_idx].name != index) ++var_idx;
    if (var_idx == arguments.size()) arguments.push_back({index, 0.0});
    auto arrays = arrayBindings.empty() ? std::map<std::string, std::vector<double>>{} : arrayBindings.back();
    arrays.erase(index);
    ScalarBindingScope scope(std::move(arguments), std::move(arrays));

    double acc = 0.0;
    if (name == "prod") acc = 1.0;
    else if (name == "min") acc = std::numeric_limits<double>::infinity();
    else if (name == "max") acc = -std::numeric_limits<double>::infinity();

    expression::ArithmeticExpression::PreparedExpression preparedBody;
    for (int64_t idx = i_start; idx <= i_end;) {
        if (loopIterationsRemaining == 0) { err = "Overflow Error: loop limit exceeded"; return std::numeric_limits<double>::quiet_NaN(); }
        --loopIterationsRemaining;
        scalarBindings.back()[var_idx].val = static_cast<double>(idx);
        const auto astValue = evaluateAst(body, err, &preparedBody);
        if (!astValue && err.empty()) err = "Math Error";
        const double step_v = astValue ? *astValue : std::numeric_limits<double>::quiet_NaN();
        if (err.empty() && engineState().g_last_eval_is_array) err = "Loop body must be scalar";
        if (!err.empty()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (name == "sum") acc += step_v;
        else if (name == "prod") acc *= step_v;
        else if (name == "min") { if (step_v < acc) acc = step_v; }
        else if (name == "max") { if (step_v > acc) acc = step_v; }
        if (idx == i_end) break;
        ++idx;
    }
    if (i_start > i_end && (name == "min" || name == "max")) acc = 0.0;
    return acc;
}

static bool acceptsBareUnary(std::string_view name);

static const expression::ArithmeticExpression::IndexedReduction indexedReduction{
    [](std::string_view index, std::string_view body) {
        const std::string name(index);
        return isValidLoopIdent(name) && (!isDefinedScalarName(name) || exprReferencesIdent(std::string(body), name));
    },
    [](std::string_view name, std::string_view index, double start, double end, std::string_view body, std::string& error) {
        return evaluateIndexedReduction(name, std::string(index), start, end, std::string(body), error);
    }, acceptsBareUnary
};

using UnaryFunction = double (*)(double);
static const std::map<std::string, UnaryFunction>& unaryFunctions() {
    static const std::map<std::string, UnaryFunction> functions = {
        {"print", +[](double value) { return value; }},
        {"sin", custom_sin},
        {"cos", custom_cos},
        {"tan", custom_tan},
        {"tg", custom_tan},
        {"cot", custom_cot},
        {"ctg", custom_cot},
        {"ctan", custom_ctan},
        {"sec", custom_sec},
        {"csc", custom_csc},
        {"cosec", custom_csc},
        {"asin", custom_asin},
        {"arcsin", custom_asin},
        {"acos", custom_acos},
        {"arccos", custom_acos},
        {"atan", custom_atan},
        {"arctan", custom_atan},
        {"atg", custom_atan},
        {"arctg", custom_atan},
        {"acot", custom_acot},
        {"arccot", custom_acot},
        {"actg", custom_acot},
        {"arcctg", custom_acot},
        {"actan", custom_acot},
        {"arcctan", custom_acot},
        {"asec", custom_asec},
        {"arcsec", custom_asec},
        {"acsc", custom_acsc},
        {"arccsc", custom_acsc},
        {"acosec", custom_acsc},
        {"arccosec", custom_acsc},
        {"sinh", custom_sinh},
        {"sh", custom_sinh},
        {"cosh", custom_cosh},
        {"ch", custom_cosh},
        {"tanh", custom_tanh},
        {"th", custom_tanh},
        {"coth", custom_coth},
        {"cth", custom_coth},
        {"ctgh", custom_coth},
        {"ctanh", custom_coth},
        {"sech", custom_sech},
        {"sch", custom_sech},
        {"csch", custom_csch},
        {"cosech", custom_csch},
        {"asinh", custom_asinh},
        {"ahsin", custom_asinh},
        {"arcsinh", custom_asinh},
        {"arsinh", custom_asinh},
        {"arsh", custom_asinh},
        {"acosh", custom_acosh},
        {"ahcos", custom_acosh},
        {"arccosh", custom_acosh},
        {"arcosh", custom_acosh},
        {"arch", custom_acosh},
        {"atanh", custom_atanh},
        {"ahtan", custom_atanh},
        {"arctanh", custom_atanh},
        {"artanh", custom_atanh},
        {"arth", custom_atanh},
        {"artg", custom_atanh},
        {"arctgh", custom_atanh},
        {"acoth", custom_acoth},
        {"ahcot", custom_acoth},
        {"arccoth", custom_acoth},
        {"arcoth", custom_acoth},
        {"arcth", custom_acoth},
        {"ahcoth", custom_acoth},
        {"asech", custom_asech},
        {"ahsec", custom_asech},
        {"arcsech", custom_asech},
        {"arsech", custom_asech},
        {"ahsech", custom_asech},
        {"acsch", custom_acsch},
        {"ahcsc", custom_acsch},
        {"arccsch", custom_acsch},
        {"arcsch", custom_acsch},
        {"acosech", custom_acsch},
        {"arccosech", custom_acsch},
        {"ahcsch", custom_acsch},
        {"deg2rad", custom_deg2rad},
        {"d2r", custom_deg2rad},
        {"rad2deg", custom_rad2deg},
        {"r2d", custom_rad2deg},
        {"sqrt", custom_sqrt},
        {"ln", custom_ln},
        {"log", custom_log10},
        {"log10", custom_log10},
        {"log2", custom_log2},
        {"sgn", custom_sgn},
        {"trunc", custom_trunc},
        {"fact", custom_fact},
        {"fib", custom_fib},
        {"abs", custom_abs},
        {"cbrt", [](double value) { return std::cbrt(value); }},
        {"exp", [](double value) { return std::exp(value); }},
        {"floor", [](double value) { return std::floor(value); }},
        {"ceil", [](double value) { return std::ceil(value); }},
    };
    return functions;
}


using BinaryFunction = expression::ArithmeticExpression::BinaryFunction;
static const std::map<std::string, BinaryFunction>& binaryFunctions() {
    static const std::map<std::string, BinaryFunction> functions = {
        {"atan2", custom_atan2}, {"logb", custom_logb}, {"mod", custom_mod},
        {"C", custom_C}, {"Cnk", custom_C}, {"P", custom_P},
        {"rUni", custom_rUni}, {"rNor", custom_rNor}, {"dot", custom_dot},
    };
    return functions;
}

struct VariadicBuiltin {
    expression::ArithmeticExpression::VariadicFunction evaluate;
};

template<te_fun24 Function>
static constexpr VariadicBuiltin variadicBuiltin{
    [](const expression::ArithmeticExpression::Arguments& args) {
        return std::apply(Function, args);
    }
};

static const std::map<std::string, VariadicBuiltin>& variadicFunctions() {
    static const std::map<std::string, VariadicBuiltin> functions = {
        {"gcd", variadicBuiltin<variadic_gcd>},
        {"lcm", variadicBuiltin<variadic_lcm>},
        {"mean", variadicBuiltin<variadic_mean>},
        {"median", variadicBuiltin<variadic_median>},
        {"mode", variadicBuiltin<variadic_mode>},
        {"var", variadicBuiltin<variadic_var>},
        {"std", variadicBuiltin<variadic_std>},
        {"sum", variadicBuiltin<variadic_sum>},
        {"prod", variadicBuiltin<variadic_prod>},
        {"min", variadicBuiltin<variadic_min>},
        {"max", variadicBuiltin<variadic_max>},
    };
    return functions;
}

static bool shadowsBuiltin(std::string_view name) {
    if (scopedScalar(name)) return true;
    const auto& state = engineState();
    for (const auto& variable : state.user_args)
        if (variable.name == name) return true;
    for (const auto& constant : state.user_consts)
        if (constant.name == name) return true;
    for (const auto& function : state.user_script_funcs)
        if (function.name == name) return true;
    for (const auto& definition : state.user_funcs)
        if (expression::trim(std::string_view(definition).substr(0, definition.find('('))) == name)
            return true;
    return false;
}

static BinaryFunction lookupBinary(std::string_view name) {
    if (shadowsBuiltin(name)) return nullptr;
    const auto found = binaryFunctions().find(std::string(name));
    return found == binaryFunctions().end() ? nullptr : found->second;
}

static UnaryFunction lookupUnary(std::string_view name) {
    if (shadowsBuiltin(name)) return nullptr;
    const auto found = unaryFunctions().find(std::string(name));
    return found == unaryFunctions().end() ? nullptr : found->second;
}

static expression::ArithmeticExpression::VariadicFunction lookupVariadic(std::string_view name) {
    if (shadowsBuiltin(name)) return nullptr;
    const auto found = variadicFunctions().find(std::string(name));
    return found == variadicFunctions().end() ? nullptr : found->second.evaluate;
}

static const std::vector<double>* lookupArray(std::string_view name) {
    if (const auto array = scopedArray(name)) return array;
    if (scopedScalar(name)) return nullptr;
    const auto& arrays = engineState().user_arrays;
    const auto found = arrays.find(std::string(name));
    return found == arrays.end() ? nullptr : &found->second;
}

static std::optional<double> lookupScalar(std::string_view name) {
    if (scopedArray(name)) return std::nullopt;
    if (const auto argument = scopedScalar(name)) return argument;
    const auto& state = engineState();
    if (name.size() > 1 && name.front() == 'e') {
        size_t number = 0;
        const auto end = name.data() + name.size();
        const auto parsed = std::from_chars(name.data() + 1, end, number);
        if (parsed.ec == std::errc{} && parsed.ptr == end &&
            number >= state.history_first_number &&
            number - state.history_first_number < state.history.size() &&
            state.history.size() - (number - state.history_first_number) <= MAX_HISTORY_VALUES &&
            name[1] != '0') {
            return state.history[number - state.history_first_number];
        }
    }
    for (auto it = state.user_consts.rbegin(); it != state.user_consts.rend(); ++it)
        if (it->name == name) return it->val;
    for (auto it = state.user_args.rbegin(); it != state.user_args.rend(); ++it)
        if (it->name == name) return it->val;
    if (name == "pi") return std::acos(-1.0);
    if (name == "e") return std::exp(1.0);
    if (state.user_arrays.contains(std::string(name)) || shadowsBuiltin(name)) return std::nullopt;
    const auto& builtins = te_parser::builtin_functions();
    const auto builtin = builtins.find(te_variable{std::string(name), 0.0});
    if (builtin != builtins.end()) {
        if (const auto function = std::get_if<te_fun0>(&builtin->m_value)) return (*function)();
    }
    return std::nullopt;
}

static bool acceptsBareUnary(std::string_view name) {
    if (shadowsBuiltin(name) || scopedArray(name)) return false;
    if (name == "round" || lookupUnary(name)) return true;
    const auto& builtins = te_parser::builtin_functions();
    const auto found = builtins.find(te_variable{std::string(name), 0.0});
    return found != builtins.end() && std::holds_alternative<te_fun1>(found->m_value);
}

static expression::ArithmeticExpression::CustomFunction lookupInheritedBuiltin(std::string_view name) {
    if (unaryFunctions().contains(std::string(name)) || binaryFunctions().contains(std::string(name)) ||
        variadicFunctions().contains(std::string(name))) return {};
    const auto& functions = te_parser::builtin_functions();
    const auto found = functions.find(te_variable{std::string(name), 0.0});
    if (found == functions.end()) return {};
    return std::visit([&](const auto function) -> expression::ArithmeticExpression::CustomFunction {
        using Function = std::decay_t<decltype(function)>;
        if constexpr (te_is_function_v<Function>) {
            constexpr size_t arity = te_function_arity<Function>;
            expression::ArithmeticExpression::CustomFunction result{arity,
                [function](const std::vector<double>& values, std::string& error) {
                    std::array<double, arity> arguments;
                    arguments.fill(std::numeric_limits<double>::quiet_NaN());
                    std::copy(values.begin(), values.end(), arguments.begin());
                    try { return std::apply(function, arguments); }
                    catch (const std::exception& failure) {
                        error = failure.what();
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                }};
            result.variadic = (found->m_type & TE_VARIADIC) != 0;
            result.acceptsNonFinite = true;
            return result;
        } else return {};
    }, found->m_value);
}

static expression::ArithmeticExpression::CustomFunction lookupCustomFunction(std::string_view name) {
    if (scopedScalar(name) || scopedArray(name)) return {};
    for (const auto& function : engineState().user_script_funcs) {
        if (function.name != name) continue;
        expression::ArithmeticExpression::CustomFunction result;
        result.arity = function.params.size();
        result.invokeArguments = [name = std::string(name)](const std::vector<expression::ArithmeticExpression::Argument>& arguments,
                std::string& error, std::optional<std::vector<double>>& array) {
            std::vector<double> values;
            std::vector<std::optional<std::vector<double>>> arrays;
            for (const auto& argument : arguments) { values.push_back(argument.scalar); arrays.push_back(argument.array); }
            // Script functions use their own parameters and session variables.
            ScalarBindingScope scope(std::vector<UserArg>{});
            return executeScriptFunc(name, values, error, nullptr, &array, &arrays);
        };
        return result;
    }
    for (const auto& definition : engineState().user_funcs) {
        const auto source = std::string_view(definition);
        if (expression::trim(source.substr(0, source.find('('))) != name) continue;
        auto parsed = parseUserFunctionDefinition(source);
        if (!parsed) continue;
        expression::ArithmeticExpression::CustomFunction result;
        result.arity = parsed->params.size();
        result.invokeArguments = [function = std::move(*parsed)](const std::vector<expression::ArithmeticExpression::Argument>& values,
                std::string& error, std::optional<std::vector<double>>& array) {
            static thread_local unsigned depth = 0;
            if (depth >= 64) {
                error = "Function recursion limit exceeded";
                return std::numeric_limits<double>::quiet_NaN();
            }
            struct CallDepth {
                unsigned& value;
                explicit CallDepth(unsigned& current) : value(current) { ++value; }
                ~CallDepth() { --value; }
            } callDepth(depth);
            std::vector<UserArg> arguments;
            std::map<std::string, std::vector<double>> arrays;
            for (size_t i = 0; i < values.size(); ++i) {
                if (values[i].array) arrays.emplace(function.params[i], *values[i].array);
                else arguments.push_back({function.params[i], values[i].scalar});
            }
            ScalarBindingScope scope(std::move(arguments), std::move(arrays));
            const double result = evaluate(function.body, error);
            if (error.empty() && engineState().g_last_eval_is_array) array = engineState().g_last_eval_array;
            return result;
        };
        return result;
    }
    return lookupInheritedBuiltin(name);
}

static std::optional<double> evaluateAst(const std::string& source, std::string& error,
        expression::ArithmeticExpression::PreparedExpression* prepared) {
    auto& state = engineState();
    state.g_math_domain_err.clear();
    state.g_last_result_angle = AngleUnit::NONE;
    state.g_last_eval_is_array = false;
    std::optional<std::vector<double>> array;
    const auto result = expression::ArithmeticExpression::evaluate(source, error, lookupScalar,
        lookupUnary, lookupArray, lookupBinary, lookupVariadic, lookupCustomFunction, &array, &indexedReduction, prepared);
    if (!result) return std::nullopt;
    if (!state.g_math_domain_err.empty()) error = state.g_math_domain_err;
    state.g_last_eval_is_array = array.has_value();
    if (array) {
        state.g_last_eval_array = std::move(*array);
    }
    return result;
}

double evaluate(const std::string& expr_str, std::string& err) {
    struct LoopBudgetScope {
        LoopBudgetScope() { if (loopEvaluationDepth++ == 0) loopIterationsRemaining = 100000; }
        ~LoopBudgetScope() { --loopEvaluationDepth; }
    } loopBudgetScope;
    err = "";
    engineState().g_math_domain_err = "";
    engineState().g_last_result_angle = AngleUnit::NONE;
    engineState().g_last_eval_is_array = false;
    std::string help_str;
    if (preprocessHelp(expr_str, help_str)) {
        err = help_str;
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    if (!checkParentheses(expr_str, err)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    
    if (const auto arithmetic = evaluateAst(expr_str, err)) return *arithmetic;

    err = "Math Error";
    return std::numeric_limits<double>::quiet_NaN();
}

double evaluateWithoutHistory(const std::string& expr_str, std::string& err) {
    // evaluate() does not append or remove history entries.
    return evaluate(expr_str, err);
}

size_t findAssignmentOperator(const std::string& text) {
    return expression::assignmentOffset(text);
}

double assignArrayElement(const std::string& name, const std::string& index,
                          const std::string& value, std::string& error) {
    auto& arrays = engineState().user_arrays;
    const auto failure = std::numeric_limits<double>::quiet_NaN();
    if (arrays.find(name) == arrays.end()) {
        error = "Array " + name + " not defined";
        return failure;
    }

    const auto position = evaluateScalar(index, error, "Array index");
    if (!error.empty()) return failure;
    auto validate = [&]() {
        const auto target = arrays.find(name);
        if (target == arrays.end()) {
            error = "Array " + name + " not defined";
        } else if (!std::isfinite(position) || std::trunc(position) != position || position < 1 ||
                   position > static_cast<double>(target->second.size())) {
            error = "Invalid or out of bounds index for array " + name +
                " (size " + std::to_string(target->second.size()) + ")";
        }
        return target;
    };
    validate();
    if (!error.empty()) return failure;
    const auto result = evaluateScalar(value, error, "Array element");
    if (!error.empty()) return failure;

    // Evaluation may call a script that replaces the target array.
    const auto target = validate();
    if (!error.empty()) return failure;
    target->second[static_cast<size_t>(position) - 1] = result;
    return result;
}

double evaluateScalar(const std::string& source, std::string& error, const char* context) {
    const double value = evaluate(source, error);
    if (error.empty() && engineState().g_last_eval_is_array) {
        error = std::string(context) + " must be scalar";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return value;
}

double evaluateInput(const std::string& expr_str, std::string& err, bool& isDefinition) {
    err = "";
    isDefinition = false;
    engineState().g_last_result_angle = AngleUnit::NONE;
    
    if (std::string name, index, value; parseArrayElementAssignment(expr_str, name, index, value)) {
        const auto result = assignArrayElement(name, index, value, err);
        isDefinition = err.empty();
        return result;
    }

    size_t eq_pos = findAssignmentOperator(expr_str);
    if (eq_pos != std::string::npos) {
        
        std::string lhs = expr_str.substr(0, eq_pos);
        std::string rhs = expr_str.substr(eq_pos + 1);
        
        lhs.erase(0, lhs.find_first_not_of(" \t"));
        lhs.erase(lhs.find_last_not_of(" \t") + 1);
        rhs.erase(0, rhs.find_first_not_of(" \t"));
        rhs.erase(rhs.find_last_not_of(" \t") + 1);
        
        bool isConstDef = false;
        if (lhs.rfind("const ", 0) == 0 || lhs.rfind("const\t", 0) == 0) {
            isConstDef = true;
            lhs = lhs.substr(5);
            lhs.erase(0, lhs.find_first_not_of(" \t"));
        }

        if (lhs.empty()) {
            err = "Invalid Name";
            return std::numeric_limits<double>::quiet_NaN();
        }
        std::string definitionName = lhs.substr(0, lhs.find('('));
        definitionName.erase(definitionName.find_last_not_of(" \t") + 1);
        if (isReservedName(definitionName)) {
            err = "Reserved name: " + definitionName;
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (lhs.find('(') == std::string::npos) {
            if (isHistoryName(lhs)) {
                err = "History variables are read-only";
                return std::numeric_limits<double>::quiet_NaN();
            }
            if (!std::isalpha(static_cast<unsigned char>(lhs[0])) && lhs[0] != '_') {
                err = "Invalid Name";
                return std::numeric_limits<double>::quiet_NaN();
            }
            for (char c : lhs) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                    err = "Invalid Name";
                    return std::numeric_limits<double>::quiet_NaN();
                }
            }
        } else {
            size_t paren = lhs.find('(');
            std::string func_name = lhs.substr(0, paren);
            func_name.erase(0, func_name.find_first_not_of(" \t"));
            func_name.erase(func_name.find_last_not_of(" \t") + 1);
            if (func_name.empty() || (!std::isalpha(static_cast<unsigned char>(func_name[0])) && func_name[0] != '_')) {
                err = "Invalid Name";
                return std::numeric_limits<double>::quiet_NaN();
            }
            for (char c : func_name) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                    err = "Invalid Name";
                    return std::numeric_limits<double>::quiet_NaN();
                }
            }
        }
        
        if (!isConstDef) {
            for (const auto& cn : engineState().user_consts) {
                if (cn.name == lhs) {
                    err = "Const Error";
                    return std::numeric_limits<double>::quiet_NaN();
                }
            }
        }
        
        isDefinition = true;
        
        if (lhs.find('(') != std::string::npos && lhs.find(')') != std::string::npos) {
            if (rhs.empty()) { err = "Func Def Error: empty body"; return std::numeric_limits<double>::quiet_NaN(); }
            size_t open_p = lhs.find('(');
            std::string target_fname = lhs.substr(0, open_p);
            target_fname.erase(0, target_fname.find_first_not_of(" \t"));
            target_fname.erase(target_fname.find_last_not_of(" \t") + 1);
            const size_t close_p = expression::matchingGroupEnd(lhs, open_p);
            if (close_p == std::string::npos) {
                err = "Func Def Error: mismatched parameters";
                return std::numeric_limits<double>::quiet_NaN();
            }
            if (!expression::trim(std::string_view(lhs).substr(close_p + 1)).empty()) {
                err = "Func Def Error: unexpected text after parameters";
                return std::numeric_limits<double>::quiet_NaN();
            }
            if (!expression::isIdentifier(target_fname)) {
                err = "Func Def Error: invalid function name";
                return std::numeric_limits<double>::quiet_NaN();
            }
            const auto parameterText = lhs.substr(open_p + 1, close_p - open_p - 1);
            const auto parameters = expression::splitArguments(parameterText);
            if (!expression::trim(parameterText).empty() && parameters.empty()) {
                err = "Func Def Error: invalid parameter list";
                return std::numeric_limits<double>::quiet_NaN();
            }
            std::set<std::string> parameterNames;
            for (const auto& parameter : parameters) {
                if (!expression::isIdentifier(parameter) || !parameterNames.insert(parameter).second) {
                    err = "Func Def Error: invalid or duplicate parameter";
                    return std::numeric_limits<double>::quiet_NaN();
                }
                if (isReservedName(parameter)) { err = "Reserved parameter: " + parameter; return std::numeric_limits<double>::quiet_NaN(); }
            }

            // Validate grammar before changing the catalogue. Definitions may
            // reference forward or recursive calls; no value is evaluated here.
            if (!expression::ArithmeticExpression::validateSyntax(rhs, &indexedReduction)) {
                err = "Func Def Error: invalid expression syntax";
                return std::numeric_limits<double>::quiet_NaN();
            }
            engineState().user_funcs.erase(std::remove_if(engineState().user_funcs.begin(), engineState().user_funcs.end(),
                [&](const std::string& definition) {
                    const auto parsed = parseUserFunctionDefinition(definition);
                    return parsed && parsed->name == target_fname;
                }), engineState().user_funcs.end());
            engineState().user_funcs.push_back(expr_str);

            std::string fname_auto = target_fname + "(";
            if (std::find(engineState().autocomplete_words.begin(), engineState().autocomplete_words.end(), fname_auto) == engineState().autocomplete_words.end()) {
                engineState().autocomplete_words.push_back(fname_auto);
            }
            return 1.0;
        } else if (isConstDef) {
            double val = evaluate(rhs, err);
            if (err.empty() && !std::isnan(val)) {
                engineState().user_args.erase(std::remove_if(engineState().user_args.begin(), engineState().user_args.end(), [&](const UserArg& a) {
                    return a.name == lhs;
                }), engineState().user_args.end());

                bool updated = false;
                for (auto& cn : engineState().user_consts) {
                    if (cn.name == lhs) {
                        cn.val = val;
                        updated = true;
                        break;
                    }
                }
                if (!updated) {
                    engineState().user_consts.push_back({lhs, val});
                }
                
                if (std::find(engineState().autocomplete_words.begin(), engineState().autocomplete_words.end(), lhs) == engineState().autocomplete_words.end()) {
                    engineState().autocomplete_words.push_back(lhs);
                }
                return val;
            } else {
                if (err.empty()) err = "Const Def Error";
                return std::numeric_limits<double>::quiet_NaN();
            }
        } else {
            bool is_arr = false;
            auto arr = parseArrayExpr(rhs, is_arr, err);
            if (err.empty() && is_arr) {
                engineState().user_arrays[lhs] = arr;
                engineState().user_args.erase(std::remove_if(engineState().user_args.begin(), engineState().user_args.end(), [&](const UserArg& a) {
                    return a.name == lhs;
                }), engineState().user_args.end());
                if (std::find(engineState().autocomplete_words.begin(), engineState().autocomplete_words.end(), lhs) == engineState().autocomplete_words.end()) {
                    engineState().autocomplete_words.push_back(lhs);
                }
                return 1.0;
            }
            if (!err.empty()) {
                return std::numeric_limits<double>::quiet_NaN();
            }
            double val;
            if (!arr.empty()) {
                val = arr.front();
                engineState().g_last_eval_is_array = false;
            } else {
                val = evaluate(rhs, err);
            }
            if (err.empty() && !std::isnan(val)) {
                if (engineState().g_last_eval_is_array) {
                    engineState().user_arrays[lhs] = engineState().g_last_eval_array;
                    engineState().user_args.erase(std::remove_if(engineState().user_args.begin(), engineState().user_args.end(), [&](const UserArg& a) {
                        return a.name == lhs;
                    }), engineState().user_args.end());
                    if (std::find(engineState().autocomplete_words.begin(), engineState().autocomplete_words.end(), lhs) == engineState().autocomplete_words.end()) {
                        engineState().autocomplete_words.push_back(lhs);
                    }
                    return 1.0;
                }
                engineState().user_arrays.erase(lhs);
                bool found = false;
                for (auto& arg : engineState().user_args) {
                    if (arg.name == lhs) {
                        arg.val = val;
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
                return val;
            } else {
                if (err.empty()) err = "Var Def Error";
                return std::numeric_limits<double>::quiet_NaN();
            }
        }
    }
    
    return evaluate(expr_str, err);
}


enum UnitDimension : uint32_t {
    DIM_NONE         = 0,
    DIM_LENGTH       = 1 << 0,
    DIM_AREA_WIRE    = 1 << 1,
    DIM_VOLUME       = 1 << 2,
    DIM_PRESSURE     = 1 << 3,
    DIM_RESISTANCE   = 1 << 4,
    DIM_CAPACITANCE  = 1 << 5,
    DIM_INDUCTANCE   = 1 << 6,
    DIM_CURRENT      = 1 << 7,
    DIM_VOLTAGE      = 1 << 8,
    DIM_POWER        = 1 << 9,
    DIM_DB_RATIO     = 1 << 10,
    DIM_FREQUENCY    = 1 << 11,
    DIM_MASS         = 1 << 12,
    DIM_TIME         = 1 << 13,
    DIM_SPEED        = 1 << 14,
    DIM_ACCELERATION = 1 << 15,
    DIM_ENERGY       = 1 << 16,
    DIM_FORCE        = 1 << 17,
    DIM_TEMPERATURE  = 1 << 18,
    DIM_ANGLE        = 1 << 19,
    DIM_EM           = 1 << 20,
};

static uint32_t get_unit_dimension_mask(const std::string& raw_unit) {
    std::string u = raw_unit;
    size_t first = u.find_first_not_of(" \t\"'");
    if (first == std::string::npos) return DIM_NONE;
    size_t last = u.find_last_not_of(" \t\"'");
    u = u.substr(first, last - first + 1);
    std::transform(u.begin(), u.end(), u.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    uint32_t dims = DIM_NONE;
    auto in_set = [&](const std::initializer_list<const char*>& items) {
        for (const char* it : items) {
            if (u == it) return true;
        }
        return false;
    };

    if (in_set({"mm", "m", "meter", "meters", "cm", "km", "mile", "miles", "yard", "yards", "foot", "feet", "inch", "inches", "in", "ft", "yd", "nauticalmile", "angstrom", "au"})) {
        dims |= DIM_LENGTH;
    }
    if (in_set({"mm^2", "mm2", "std_mm2", "awg", "std_awg", "m^2", "m2", "cm^2", "cm2", "km^2", "km2", "hectare", "acre", "acres", "sqft", "sqin"})) {
        dims |= DIM_AREA_WIRE;
    }
    if (in_set({"m^3", "m3", "cm^3", "cm3", "liter", "liters", "litre", "litres", "l", "ml", "dl", "cl", "gal", "gallon", "gallons", "quart", "quarts", "pint", "pints", "cup", "cups", "floz", "tbsp", "tsp", "barrel"})) {
        dims |= DIM_VOLUME;
    }
    if (in_set({"mmhg", "torr", "atm", "bar", "mbar", "pa", "pascal", "kpa", "mpa", "gpa", "psi", "psf", "inhg"})) {
        dims |= DIM_PRESSURE;
    }
    if (in_set({"ohm", "ohms", "mohm", "kohm", "smd3", "smd4", "eia96"})) {
        dims |= DIM_RESISTANCE;
    } else if (!u.empty()) {
        bool all_digits = std::all_of(u.begin(), u.end(), [](unsigned char c) { return std::isdigit(c); });
        if ((u.size() == 3 || u.size() == 4) && all_digits) {
            dims |= DIM_RESISTANCE;
        } else if (u.find('r') != std::string::npos || u.find('k') != std::string::npos) {
            bool is_rkm = true;
            for (char c : u) {
                if (!std::isdigit((unsigned char)c) && c != 'r' && c != 'k') { is_rkm = false; break; }
            }
            if (is_rkm) dims |= DIM_RESISTANCE;
        }
    }
    if (in_set({"farad", "f", "uf", "nf", "pf"})) {
        dims |= DIM_CAPACITANCE;
    }
    if (in_set({"henry", "h", "uh", "mh", "nh"})) {
        dims |= DIM_INDUCTANCE;
    }
    if (in_set({"ampere", "amp", "amps", "a", "ma", "ua", "ka"})) {
        dims |= DIM_CURRENT;
    }
    if (in_set({"volt", "volts", "v", "mv", "uv", "kv", "dbv", "dbmv", "dbuv", "dbu"})) {
        dims |= DIM_VOLTAGE;
    }
    if (in_set({"watt", "watts", "w", "mw", "uw", "kw", "gw", "hp", "horsepower", "dbm", "dbw"})) {
        dims |= DIM_POWER;
    }
    if (in_set({"db", "times", "ratio", "times_amp", "times_v", "db_amp", "db_v", "dbm", "dbw", "dbv", "dbmv", "dbuv", "dbu"})) {
        dims |= DIM_DB_RATIO;
    }
    if (in_set({"hz", "khz", "mhz", "ghz"})) {
        dims |= DIM_FREQUENCY;
    }
    if (in_set({"kg", "kilogram", "kilograms", "g", "gram", "grams", "mg", "ug", "ton", "tonne", "tonnes", "lb", "pound", "pounds", "oz", "ounce", "ounces", "stone", "carat"})) {
        dims |= DIM_MASS;
    }
    if (in_set({"s", "sec", "second", "seconds", "ms", "us", "ns", "min", "minute", "minutes", "hr", "hour", "hours", "day", "days", "week", "weeks", "month", "months", "year", "years"})) {
        dims |= DIM_TIME;
    }
    if (in_set({"m/s", "km/hr", "km/h", "kph", "mph", "knot", "knots", "mach"})) {
        dims |= DIM_SPEED;
    }
    if (in_set({"m/s^2", "m/s2", "cm/s^2", "cm/s2", "km/s^2", "km/s2", "km/h^2", "km/h2", "km/hr^2", "km/hr2", "ft/s^2", "ft/s2", "fps2", "in/s^2", "in/s2", "gal", "g0", "gee", "gravity", "g"})) {
        dims |= DIM_ACCELERATION;
    }
    if (in_set({"joule", "joules", "j", "kj", "mj", "cal", "calorie", "calories", "kcal", "ev", "kev", "mev", "gev", "wh", "kwh", "mwh", "btu"})) {
        dims |= DIM_ENERGY;
    }
    if (in_set({"newton", "newtons", "n", "kn", "dyn", "dyne", "lbf", "kgf"})) {
        dims |= DIM_FORCE;
    }
    if (in_set({"degc", "degf", "kelvin", "k", "tempc", "tempf", "tempk", "tempr", "degr", "celsius", "fahrenheit", "rankine"})) {
        dims |= DIM_TEMPERATURE;
    }
    if (in_set({"deg", "degree", "degrees", "rad", "radian", "radians"})) {
        dims |= DIM_ANGLE;
    }
    if (in_set({"coulomb", "tesla", "weber"})) {
        dims |= DIM_EM;
    }
    return dims;
}

struct ConvUnitCandidate {
    const char* name;
    uint32_t dims;
};

static const ConvUnitCandidate conv_units[] = {
    {"mm", DIM_LENGTH}, {"mmHg", DIM_PRESSURE}, {"mm^2", DIM_AREA_WIRE}, {"mm2", DIM_AREA_WIRE},
    {"std_mm2", DIM_AREA_WIRE}, {"m", DIM_LENGTH}, {"meter", DIM_LENGTH}, {"cm", DIM_LENGTH},
    {"km", DIM_LENGTH}, {"mile", DIM_LENGTH}, {"yard", DIM_LENGTH}, {"foot", DIM_LENGTH},
    {"inch", DIM_LENGTH}, {"in", DIM_LENGTH}, {"ft", DIM_LENGTH}, {"yd", DIM_LENGTH},
    {"m^2", DIM_AREA_WIRE}, {"m2", DIM_AREA_WIRE}, {"cm^2", DIM_AREA_WIRE}, {"cm2", DIM_AREA_WIRE},
    {"km^2", DIM_AREA_WIRE}, {"km2", DIM_AREA_WIRE}, {"hectare", DIM_AREA_WIRE}, {"acre", DIM_AREA_WIRE},
    {"m^3", DIM_VOLUME}, {"m3", DIM_VOLUME}, {"liter", DIM_VOLUME}, {"l", DIM_VOLUME},
    {"ml", DIM_VOLUME}, {"dl", DIM_VOLUME}, {"cl", DIM_VOLUME}, {"gal", DIM_VOLUME},
    {"gallon", DIM_VOLUME}, {"quart", DIM_VOLUME}, {"pint", DIM_VOLUME}, {"cup", DIM_VOLUME},
    {"floz", DIM_VOLUME}, {"tbsp", DIM_VOLUME}, {"tsp", DIM_VOLUME}, {"barrel", DIM_VOLUME},
    {"torr", DIM_PRESSURE}, {"atm", DIM_PRESSURE}, {"bar", DIM_PRESSURE}, {"mbar", DIM_PRESSURE},
    {"Pa", DIM_PRESSURE}, {"pascal", DIM_PRESSURE}, {"kPa", DIM_PRESSURE}, {"MPa", DIM_PRESSURE},
    {"GPa", DIM_PRESSURE}, {"psi", DIM_PRESSURE}, {"inHg", DIM_PRESSURE},
    {"ohm", DIM_RESISTANCE}, {"mohm", DIM_RESISTANCE}, {"kohm", DIM_RESISTANCE}, {"Mohm", DIM_RESISTANCE},
    {"smd3", DIM_RESISTANCE}, {"smd4", DIM_RESISTANCE}, {"eia96", DIM_RESISTANCE},
    {"awg", DIM_AREA_WIRE}, {"std_awg", DIM_AREA_WIRE},
    {"farad", DIM_CAPACITANCE}, {"F", DIM_CAPACITANCE}, {"uF", DIM_CAPACITANCE}, {"uf", DIM_CAPACITANCE},
    {"nF", DIM_CAPACITANCE}, {"nf", DIM_CAPACITANCE}, {"pF", DIM_CAPACITANCE}, {"pf", DIM_CAPACITANCE},
    {"henry", DIM_INDUCTANCE}, {"H", DIM_INDUCTANCE}, {"uH", DIM_INDUCTANCE}, {"uh", DIM_INDUCTANCE},
    {"mH", DIM_INDUCTANCE}, {"mh", DIM_INDUCTANCE}, {"nH", DIM_INDUCTANCE}, {"nh", DIM_INDUCTANCE},
    {"ampere", DIM_CURRENT}, {"amp", DIM_CURRENT}, {"A", DIM_CURRENT}, {"mA", DIM_CURRENT},
    {"uA", DIM_CURRENT}, {"kA", DIM_CURRENT},
    {"volt", DIM_VOLTAGE}, {"V", DIM_VOLTAGE}, {"mV", DIM_VOLTAGE}, {"uV", DIM_VOLTAGE},
    {"kV", DIM_VOLTAGE}, {"MV", DIM_VOLTAGE},
    {"watt", DIM_POWER}, {"W", DIM_POWER}, {"mW", DIM_POWER}, {"uW", DIM_POWER},
    {"kW", DIM_POWER}, {"MW", DIM_POWER}, {"GW", DIM_POWER}, {"hp", DIM_POWER},
    {"Hz", DIM_FREQUENCY}, {"khz", DIM_FREQUENCY}, {"mhz", DIM_FREQUENCY}, {"ghz", DIM_FREQUENCY},
    {"dB", DIM_DB_RATIO}, {"db", DIM_DB_RATIO},
    {"dBm", DIM_POWER | DIM_DB_RATIO}, {"dbm", DIM_POWER | DIM_DB_RATIO},
    {"dBW", DIM_POWER | DIM_DB_RATIO}, {"dbw", DIM_POWER | DIM_DB_RATIO},
    {"dBV", DIM_VOLTAGE | DIM_DB_RATIO}, {"dbv", DIM_VOLTAGE | DIM_DB_RATIO},
    {"dBmV", DIM_VOLTAGE | DIM_DB_RATIO}, {"dbmv", DIM_VOLTAGE | DIM_DB_RATIO},
    {"dBuV", DIM_VOLTAGE | DIM_DB_RATIO}, {"dbuv", DIM_VOLTAGE | DIM_DB_RATIO},
    {"dBu", DIM_VOLTAGE | DIM_DB_RATIO}, {"dbu", DIM_VOLTAGE | DIM_DB_RATIO},
    {"times", DIM_DB_RATIO}, {"ratio", DIM_DB_RATIO},
    {"kg", DIM_MASS}, {"kilogram", DIM_MASS}, {"g", DIM_MASS}, {"gram", DIM_MASS},
    {"mg", DIM_MASS}, {"ug", DIM_MASS}, {"ton", DIM_MASS}, {"tonne", DIM_MASS},
    {"lb", DIM_MASS}, {"pound", DIM_MASS}, {"oz", DIM_MASS}, {"ounce", DIM_MASS},
    {"s", DIM_TIME}, {"sec", DIM_TIME}, {"second", DIM_TIME}, {"ms", DIM_TIME},
    {"us", DIM_TIME}, {"ns", DIM_TIME}, {"min", DIM_TIME}, {"minute", DIM_TIME},
    {"hr", DIM_TIME}, {"hour", DIM_TIME}, {"day", DIM_TIME}, {"week", DIM_TIME},
    {"month", DIM_TIME}, {"year", DIM_TIME},
    {"m/s", DIM_SPEED}, {"km/hr", DIM_SPEED}, {"km/h", DIM_SPEED}, {"kph", DIM_SPEED},
    {"mph", DIM_SPEED}, {"knot", DIM_SPEED}, {"mach", DIM_SPEED},
    {"m/s^2", DIM_ACCELERATION}, {"cm/s^2", DIM_ACCELERATION}, {"km/s^2", DIM_ACCELERATION},
    {"km/h^2", DIM_ACCELERATION}, {"ft/s^2", DIM_ACCELERATION}, {"in/s^2", DIM_ACCELERATION},
    {"gal", DIM_ACCELERATION}, {"g0", DIM_ACCELERATION}, {"gee", DIM_ACCELERATION}, {"gravity", DIM_ACCELERATION},
    {"joule", DIM_ENERGY}, {"J", DIM_ENERGY}, {"kJ", DIM_ENERGY}, {"MJ", DIM_ENERGY},
    {"cal", DIM_ENERGY}, {"calorie", DIM_ENERGY}, {"kcal", DIM_ENERGY},
    {"eV", DIM_ENERGY}, {"keV", DIM_ENERGY}, {"MeV", DIM_ENERGY}, {"GeV", DIM_ENERGY},
    {"Wh", DIM_ENERGY}, {"kWh", DIM_ENERGY}, {"MWh", DIM_ENERGY}, {"BTU", DIM_ENERGY},
    {"newton", DIM_FORCE}, {"N", DIM_FORCE}, {"kN", DIM_FORCE}, {"dyn", DIM_FORCE},
    {"dyne", DIM_FORCE}, {"lbf", DIM_FORCE}, {"kgf", DIM_FORCE},
    {"degC", DIM_TEMPERATURE}, {"degF", DIM_TEMPERATURE}, {"kelvin", DIM_TEMPERATURE}, {"K", DIM_TEMPERATURE},
    {"deg", DIM_ANGLE}, {"degree", DIM_ANGLE}, {"rad", DIM_ANGLE}, {"radian", DIM_ANGLE},
    {"nauticalmile", DIM_LENGTH}, {"angstrom", DIM_LENGTH}, {"au", DIM_LENGTH},
    {"sqft", DIM_AREA_WIRE}, {"sqin", DIM_AREA_WIRE},
    {"cm^3", DIM_VOLUME}, {"cm3", DIM_VOLUME}, {"litre", DIM_VOLUME},
    {"psf", DIM_PRESSURE}, {"horsepower", DIM_POWER},
    {"coulomb", DIM_EM}, {"tesla", DIM_EM}, {"weber", DIM_EM},
    {"stone", DIM_MASS}, {"carat", DIM_MASS},
    {"tempC", DIM_TEMPERATURE}, {"tempF", DIM_TEMPERATURE}, {"tempK", DIM_TEMPERATURE}, {"tempR", DIM_TEMPERATURE}, {"degR", DIM_TEMPERATURE}
};

std::vector<std::string> getConvUnitMatches(const std::string& from_unit, const std::string& prefix) {
    uint32_t from_mask = get_unit_dimension_mask(from_unit);
    std::vector<std::string> matches;
    for (const auto& u : conv_units) {
        if (from_mask != DIM_NONE && (u.dims & from_mask) == 0) {
            continue;
        }
        std::string word = u.name;
        if (word.size() >= prefix.size()) {
            bool match = true;
            for (size_t i = 0; i < prefix.size(); ++i) {
                if (std::tolower((unsigned char)word[i]) != std::tolower((unsigned char)prefix[i])) {
                    match = false;
                    break;
                }
            }
            if (match && std::find(matches.begin(), matches.end(), word) == matches.end()) {
                matches.push_back(word);
            }
        }
    }
    return matches;
}

void resetTabState() {
    engineState().g_tab_orig_prefix = "";
    engineState().g_tab_last_inserted = "";
    engineState().g_tab_comp_start = 0;
    engineState().g_tab_match_idx = -1;
    engineState().g_tab_cached_matches.clear();
}

void handleTabCompletion(std::string& expression, int& cursor_pos) {
    if (expression.empty() && cursor_pos == -999) {
        resetTabState();
        return;
    }

    bool is_continuation = false;
    if (!engineState().g_tab_orig_prefix.empty() && !engineState().g_tab_last_inserted.empty() && engineState().g_tab_comp_start <= expression.size()) {
        if (engineState().g_tab_comp_start + engineState().g_tab_last_inserted.size() <= expression.size() &&
            expression.substr(engineState().g_tab_comp_start, engineState().g_tab_last_inserted.size()) == engineState().g_tab_last_inserted) {
            if (cursor_pos >= (int)engineState().g_tab_comp_start && cursor_pos <= (int)(engineState().g_tab_comp_start + engineState().g_tab_last_inserted.size())) {
                is_continuation = true;
            }
        }
    }

    if (is_continuation && !engineState().g_tab_cached_matches.empty()) {
        engineState().g_tab_match_idx = ((engineState().g_tab_match_idx + 1) % (int)engineState().g_tab_cached_matches.size() + (int)engineState().g_tab_cached_matches.size()) % (int)engineState().g_tab_cached_matches.size();
        std::string matched_word = engineState().g_tab_cached_matches[engineState().g_tab_match_idx];

        std::string new_inserted;
        if (engineState().auto_brackets && !matched_word.empty() && matched_word.back() == '(') {
            new_inserted = matched_word + ")";
            cursor_pos = engineState().g_tab_comp_start + matched_word.size();
        } else {
            new_inserted = matched_word;
            cursor_pos = engineState().g_tab_comp_start + new_inserted.size();
        }

        size_t current_len = engineState().g_tab_last_inserted.size();
        if (engineState().g_tab_comp_start + current_len > expression.size()) {
            current_len = expression.size() - engineState().g_tab_comp_start;
        }
        expression.replace(engineState().g_tab_comp_start, current_len, new_inserted);
        engineState().g_tab_last_inserted = new_inserted;
        return;
    }

    if (!is_continuation) {
        engineState().g_tab_cached_matches.clear();
        if (cursor_pos < 0 || cursor_pos > (int)expression.size()) cursor_pos = expression.size();
        size_t start = cursor_pos;
        while (start > 0 && (std::isalnum((unsigned char)expression[start - 1]) || expression[start - 1] == '_')) {
            start--;
        }
        std::string prefix = expression.substr(start, cursor_pos - start);
        if (prefix.empty()) {
            resetTabState();
            return;
        }

        engineState().g_tab_orig_prefix = prefix;
        engineState().g_tab_comp_start = start;
        engineState().g_tab_match_idx = 0;
    } else {
        engineState().g_tab_match_idx++;
    }

    std::vector<std::string> matches;
    matches.reserve(16);

    auto check_candidate = [&](const std::string& word) {
        if (word.rfind(engineState().g_tab_orig_prefix, 0) == 0) {
            if (std::find(matches.begin(), matches.end(), word) == matches.end()) {
                matches.push_back(word);
            }
        }
    };

    // Check if inside conv(...) and specifically in a unit argument position (after comma)
    bool is_in_conv = false;
    std::string from_unit = "";
    std::string left_ctx = expression.substr(0, engineState().g_tab_comp_start);
    size_t last_conv = left_ctx.rfind("conv(");
    if (last_conv != std::string::npos) {
        std::string after_conv = left_ctx.substr(last_conv + 5);
        if (after_conv.find(')') == std::string::npos) {
            is_in_conv = true;
            std::vector<std::string> args;
            std::string cur_arg;
            bool in_q = false;
            char q_char = 0;
            for (char ch : after_conv) {
                if ((ch == '"' || ch == '\'') && (!in_q || ch == q_char)) {
                    in_q = !in_q;
                    q_char = in_q ? ch : 0;
                    cur_arg += ch;
                } else if (ch == ',' && !in_q) {
                    args.push_back(cur_arg);
                    cur_arg.clear();
                } else {
                    cur_arg += ch;
                }
            }
            if (args.size() >= 2) {
                from_unit = args[args.size() - 1];
            } else if (args.size() == 1) {
                std::string a0 = args[0];
                size_t f = a0.find_first_not_of(" \t\"'");
                size_t l = a0.find_last_not_of(" \t\"'");
                if (f != std::string::npos && l != std::string::npos) {
                    std::string s = a0.substr(f, l - f + 1);
                    size_t p = s.find_last_of(" 0123456789+-*/^");
                    if (p != std::string::npos && p + 1 < s.size() && std::isalpha((unsigned char)s[p + 1])) {
                        from_unit = s.substr(p + 1);
                    }
                }
            }
        }
    }

    if (is_in_conv && left_ctx.substr(last_conv + 5).find(',') != std::string::npos) {
        matches = getConvUnitMatches(from_unit, engineState().g_tab_orig_prefix);
    }

    if (matches.empty()) {
        // 1. Built-in constants (pi, e, phi) & User variables/constants first
        for (const auto& w : engineState().autocomplete_words) {
            if (w == "pi" || w == "e" || w == "phi") check_candidate(w);
        }
        for (const auto& arg : engineState().user_args) check_candidate(arg.name);
        for (const auto& cn : engineState().user_consts) check_candidate(cn.name);

        // 2. Built-in math functions & history variables (e1, e2, e3...)
        for (const auto& w : engineState().autocomplete_words) {
            if (w != "pi" && w != "e" && w != "phi") check_candidate(w);
        }
        for (size_t i = 0; i < engineState().history.size(); ++i) check_candidate("e" + std::to_string(engineState().history_first_number + i));

        // 3. User formulas & scripts
        for (const auto& func : engineState().user_funcs) {
            size_t paren = func.find('(');
            if (paren != std::string::npos) {
                std::string fname = func.substr(0, paren + 1);
                if (!fname.empty()) check_candidate(fname);
            }
        }
        for (const auto& sf : engineState().user_script_funcs) {
            check_candidate(sf.name + "(");
        }
    }

    if (matches.empty()) {
        resetTabState();
        return;
    }

    engineState().g_tab_cached_matches = matches;
    engineState().g_tab_match_idx = ((engineState().g_tab_match_idx % (int)matches.size()) + (int)matches.size()) % (int)matches.size();
    std::string matched_word = matches[engineState().g_tab_match_idx];

    std::string new_inserted;
    if (engineState().auto_brackets && !matched_word.empty() && matched_word.back() == '(') {
        new_inserted = matched_word + ")";
        cursor_pos = engineState().g_tab_comp_start + matched_word.size();
    } else {
        new_inserted = matched_word;
        cursor_pos = engineState().g_tab_comp_start + new_inserted.size();
    }

    if (is_continuation) {
        size_t current_len = engineState().g_tab_last_inserted.size();
        if (engineState().g_tab_comp_start + current_len > expression.size()) {
            current_len = expression.size() - engineState().g_tab_comp_start;
        }
        expression.replace(engineState().g_tab_comp_start, current_len, new_inserted);
    } else {
        expression.replace(engineState().g_tab_comp_start, engineState().g_tab_orig_prefix.size(), new_inserted);
    }
    engineState().g_tab_last_inserted = new_inserted;
}

std::vector<double> parseArrayExpr(const std::string& source, bool& isArray, std::string& error) {
    const auto trimmed = expression::trim(source);
    isArray = !trimmed.empty() && trimmed.front() == '[' && expression::matchingGroupEnd(trimmed, 0) == trimmed.size() - 1;
    const double value = evaluate(source, error);
    if (!error.empty()) return {};
    isArray = engineState().g_last_eval_is_array;
    return isArray ? engineState().g_last_eval_array : std::vector<double>{value};
}

std::string formatPrintString(const std::string& str, std::string& err) {
    const auto formatValue = [&](const std::string& expression) {
        const double value = evaluate(expression, err);
        if (!err.empty()) return std::string{};
        if (!engineState().g_last_eval_is_array) return fmtNum(value);
        std::string output = "[";
        const auto& values = engineState().g_last_eval_array;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i) output += ", ";
            output += fmtNum(values[i]);
        }
        return output + "]";
    };
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        std::string inner = str.substr(1, str.size() - 2);
        std::string res = "";
        size_t i = 0;
        while (i < inner.size()) {
            if (inner[i] == '\\' && i + 1 < inner.size()) { res += inner[i + 1]; i += 2; continue; }
            if (inner[i] == '"') { err = "Invalid string expression"; return {}; }
            if (inner[i] == '{') {
                size_t close = inner.find('}', i);
                if (close == std::string::npos) {
                    err = "Mismatched braces in print";
                    return str;
                }
                std::string expr = inner.substr(i + 1, close - i - 1);
                expr.erase(0, expr.find_first_not_of(" \t"));
                expr.erase(expr.find_last_not_of(" \t") + 1);

                res += formatValue(expr);
                if (!err.empty()) return {};
                i = close + 1;
            } else {
                res += inner[i++];
            }
        }
        return res;
    } else {
        return formatValue(str);
    }
}

bool parseArrayElementAssignment(const std::string& line, std::string& name, std::string& idx_expr, std::string& val_expr) {
    const auto tokens = expression::tokenize(line);
    using expression::Kind;
    if (tokens.size() < 6 || tokens[0].kind != Kind::Word || tokens[1].text != "[")
        return false;
    std::vector<char> groups{'['};
    for (size_t i = 2; i + 2 < tokens.size(); ++i) {
        const auto token = tokens[i];
        if (token.kind == Kind::Open) groups.push_back(token.text.front());
        else if (token.kind == Kind::Close && !expression::closeGroup(groups, token.text.front()))
            return false;
        if (!groups.empty()) continue;
        if (i == 2 || tokens[i + 1].text != "=") return false;
        name = tokens[0].text;
        const auto start = tokens[1].start + 1;
        idx_expr = line.substr(start, token.start - start);
        val_expr = line.substr(tokens[i + 1].start + 1);
        return true;
    }
    return false;
}
