#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <thread>
#include <atomic>
extern "C" int bridge_test_fatal();


#include "engine/app_state.h"
#include "engine/help_engine.h"
#include "engine/math_funcs.h"
#include "engine/plot_engine.h"
#include "engine/script_engine.h"
#include "engine/math_engine.h"
#include "engine/units_adapter.h"

#include "engine/help_engine.cpp"
#include "engine/math_engine.cpp"
#include "engine/plot_engine.cpp"
#include "engine/script_engine.cpp"

#include "engine/app_state.cpp"

int main() {
    // Exercise the same index and topic renderer used by the Android Help screen.
    std::string helpIndex;
    assert(preprocessHelp("help(index)", helpIndex));
    for (const auto& entry : helpEntries) {
        std::string rendered;
        assert(helpIndex.find(std::string(entry.name) + "\n") != std::string::npos);
        assert(preprocessHelp("help(\"" + std::string(entry.name) + "\")", rendered));
        assert(rendered.find("\n\nSyntax\n") != std::string::npos);
        assert(rendered.find("\n\nExamples\n") != std::string::npos);
        assert(rendered.find("General math function/variable") == std::string::npos);
    }
    std::string xorHelp;
    assert(preprocessHelp("help(XOR)", xorHelp));
    assert(xorHelp.find("2 xor 3 = 0") != std::string::npos);
    std::cout << "[Droidulator Native Test] Initializing..." << std::endl;
    std::string err;
    EngineContext firstSession;
    EngineContext secondSession;
    {
        EngineContextScope first(firstSession);
        bool defined = false;
        evaluateInput("isolated_var=42", err, defined);
        assert(defined && err.empty());
        appendHistory(7);
        engineState().degreesMode = false;
        {
            EngineContextScope second(secondSession);
            assert(engineState().degreesMode && engineState().history.empty());
            evaluate("isolated_var", err);
            assert(!err.empty());
            err.clear();
            evaluateInput("isolated_var=3", err, defined);
            assert(defined && err.empty());
        }
        assert(evaluate("isolated_var", err) == 42);
        assert(engineState().history.size() == 1 && engineState().history[0] == 7);
        assert(evaluate("isolated_var * 2 + e1", err) == 91 && err.empty());
        engineState().user_arrays["ast_samples"] = {2, 4, 8};
        assert(expression::ArithmeticExpression::evaluate("ast_samples[1+1] * 2", err,
            lookupScalar, lookupUnary, lookupArray) == 8 && err.empty());
        assert(expression::ArithmeticExpression::evaluate("len(ast_samples) + ast_samples[2]", err,
            lookupScalar, lookupUnary, lookupArray) == 7 && err.empty());
        engineState().user_arrays["ast_empty"] = {};
        assert(evaluate("len(ast_empty)", err) == 0 && err.empty());
        assert(std::isnan(evaluate("ast_samples[1.4]", err)) && err.find("out of bounds") != std::string::npos);
        err.clear();
        assert(evaluate("-(isolated_var + e1)", err) == -49 && err.empty());
    }
    {
        EngineContextScope second(secondSession);
        assert(evaluate("isolated_var", err) == 3);
    }
    assert(findAssignmentOperator("x >= 2") == std::string::npos);
    assert(findAssignmentOperator("x += 2") == std::string::npos);
    assert(findAssignmentOperator("x = f(1, 2)") == 2);
    assert(findAssignmentOperator(R"(print("a = b"))") == std::string::npos);
    assert(expression::callBody(R"(conv (max(1, 2), "m", "cm"))", "conv").has_value());
    assert(!expression::callBody("conv(1, m) + f(2)", "conv"));
    assert(!expression::callBody("conv([1), m)", "conv"));
    std::string arrayName, indexExpression, assignedValue;
    assert(parseArrayElementAssignment("a[b[1] + 1] = 7", arrayName, indexExpression, assignedValue));
    assert(arrayName == "a" && indexExpression == "b[1] + 1" && expression::trim(assignedValue) == "7");
    assert(!parseArrayElementAssignment("a[1] == 7", arrayName, indexExpression, assignedValue));
    assert(!parseArrayElementAssignment("a[1) = 7", arrayName, indexExpression, assignedValue));
    {
        EngineContext arrays;
        EngineContextScope scope(arrays);
        arrays.user_arrays["a"] = {10, 20};
        arrays.user_arrays["b"] = {1};
        bool defined = false;
        assert(evaluateInput("a[b[1]+1] = 7", err, defined) == 7 && defined && err.empty());
        assert(arrays.user_arrays.at("a")[1] == 7);
        evaluateInput("a[1.4] = 9", err, defined);
        assert(!defined && !err.empty() && arrays.user_arrays.at("a")[0] == 10);
        runScript("a[b[1]+1] = 8");
        assert(arrays.user_arrays.at("a")[1] == 8);
        err.clear();
    }
    const auto arguments = expression::splitArguments(R"(max(1, 2), 'm, x', [1,2])");
    assert((arguments == std::vector<std::string>{"max(1, 2)", "'m, x'", "[1,2]"}));
    assert(expression::splitArguments("x, [1, 2)").empty());
    const std::pair<const char*, double> arithmeticCases[] = {
        {"2+3*4", 14}, {"(2+3)*4", 20}, {"2^3^2", 64}, {"-2^2", 4},
        {"2^-3", 0.125}, {"-(1+.5e+1)", -6},
        {"2+3 >= 5 and 2 != 3", 1}, {"0 or 2 xor 3", 0}, {"1 || 0 && 0", 1},
        {"0 < 1 < 2", 1}, {"1 == 2 < 3", 1}
    };
    for (const auto& [text, expected] : arithmeticCases) {
        assert(expression::ArithmeticExpression::evaluate(text, err) == expected);
        assert(evaluate(text, err) == expected && err.empty());
    }
    assert(expression::ArithmeticExpression::evaluate("abs(-sqrt(9))+fact(3)", err,
        lookupScalar, lookupUnary) == 9 && err.empty());
    assert(evaluate("sin(30)+cos(60)", err) > 0.999999 && err.empty());
    assert(std::isnan(evaluate("sqrt(-1)", err)) && err.find("Domain Error") != std::string::npos);
    err.clear();
    assert(expression::ArithmeticExpression::evaluate("-3! + (2+1)! * 2!", err,
        lookupScalar, lookupUnary) == 6 && err.empty());
    assert(evaluate("3!!", err) == 720 && err.empty());
    assert(evaluate("2^3!", err) == 64 && err.empty());
    assert(std::isnan(evaluate("(-3)!", err)) && err.find("Domain Error") != std::string::npos);
    err.clear();
    assert(expression::ArithmeticExpression::evaluate("logb(8,2)+C(5,2)+mod(7,3)", err,
        lookupScalar, lookupUnary, lookupArray, lookupBinary) == 14 && err.empty());
    assert(evaluate("dot(3,4)+P(3,2)", err) == 18 && err.empty());
    assert(std::isnan(evaluate("mod(1,0)", err)) && err == "Division by Zero");
    err.clear();
    assert(expression::ArithmeticExpression::evaluate("sum(1,mean(2,4),gcd(8,12,16))", err,
        lookupScalar, lookupUnary, lookupArray, lookupBinary, lookupVariadic) == 8 && err.empty());
    assert(evaluate("sum(ast_index, 1, 3, ast_index^2)", err) == 14 && err.empty());
    assert(evaluate("sum(ast_index, 1, 3, ast_index^2) > 10 and 0 != 1", err) == 1 && err.empty());
    assert(!expression::ArithmeticExpression::evaluate("sin(0)", err));
    assert(std::isnan(evaluate("1 / (2-2)", err)) && err == "Division by Zero");
    err.clear();
    const std::pair<const char*, double> rkmCases[] = {
        {"4k7", 4700}, {"4k70", 4700}, {"4k75", 4750}, {"1k50", 1500},
        {"2m20", 0.0022}, {"4k075", 4075}, {"1 / 4k75", 1.0 / 4750}
    };
    for (const auto& [expression, expected] : rkmCases) {
        const auto decimal = expression::ArithmeticExpression::evaluate(expression, err);
        assert(decimal && std::abs(*decimal - expected) < 1e-9);
        assert(std::abs(evaluate(expression, err) - expected) < 1e-9);
        assert(err.empty());
    }

    bool isDef = false;

    // 1. Basic math test
    double res = evaluateInput("2 + 3 * 4", err, isDef);
    std::cout << "2 + 3 * 4 = " << res << std::endl;
    assert(res == 14.0);

    // 2. Engineering suffix test
    double res2 = evaluateInput("1k + 500", err, isDef);
    std::cout << "1k + 500 = " << res2 << std::endl;
    assert(res2 == 1500.0);

    auto expectNumber = [](const std::string& expression, double expected) {
        std::string error;
        double actual = evaluate(expression, error);
        if (!error.empty() || std::abs(actual - expected) > std::max(1e-10, std::abs(expected) * 1e-9)) {
            std::cerr << "Unexpected result: " << expression << " = " << actual << "; " << error << std::endl;
            assert(false);
        }
    };
    expectNumber("1 / 1e5", 1e-5);
    expectNumber("1 / 10k", 1e-4);
    {
        std::string error; bool isDefinition = false;
        engineState().user_script_funcs.push_back({"recursive_test", {"x"}, {"return recursive_test(x)"}});
        evaluate("recursive_test(1)", error);
        assert(error.find("recursion limit") != std::string::npos);
        engineState().user_script_funcs.clear();
        evaluateInput("x=3", error, isDefinition); assert(error.empty());
        evaluateInput("flag=(x==3)", error, isDefinition); assert(error.empty());
        expectNumber("flag", 1);
        evaluateInput("ok=x>=2", error, isDefinition); assert(error.empty());
        expectNumber("ok", 1);
        evaluateInput("q(x)=x>=0", error, isDefinition); assert(error.empty());
        expectNumber("q(2)", 1); expectNumber("q(-2)", 0);
        evaluateInput("a=[1,2,4]", error, isDefinition); assert(error.empty());
        bool isArray = false;
        auto nested = parseArrayExpr("[atan2(1,1), 2]", isArray, error);
        assert(error.empty() && isArray && nested.size() == 2);
        assert(std::abs(nested[1] - 2) < 1e-12);
        auto range = parseArrayExpr("[0:0.1:1]", isArray, error); assert(error.empty() && isArray && range.size() == 11);
        auto inverse = parseArrayExpr("1 ./ a", isArray, error); assert(error.empty() && isArray && inverse.size() == 3 && inverse[2] == .25);
        auto scaled = parseArrayExpr("a * -2", isArray, error); assert(error.empty() && isArray && scaled[1] == -4);
        scaled = parseArrayExpr("1e-3 * a", isArray, error); assert(error.empty() && isArray && std::abs(scaled[1] - .002) < 1e-12);
        scaled = parseArrayExpr("a * 1e-3", isArray, error); assert(error.empty() && isArray && std::abs(scaled[1] - .002) < 1e-12);
        evaluate("a[1.4]", error); assert(!error.empty());
        formatPrintString("\"x = {zzz} ok\"", error); assert(!error.empty());
        error.clear(); formatPrintString("\"a\" + \"b\"", error); assert(!error.empty());
        evaluateInput("empty(x)=", error, isDefinition); assert(!error.empty());
        evaluate("rNor(0,-1)", error); assert(!error.empty());
        std::string modError, percentError;
        evaluate("mod(1,0)", modError); evaluate("1%0", percentError);
        assert(modError == "Division by Zero" && percentError == modError);
        assert(evaluate("17%5*2", percentError) == 4 && percentError.empty());
        assert(evaluate("17%(2+3)", percentError) == 2 && percentError.empty());
        assert(evaluate("-17%5", percentError) == -2 && percentError.empty());
        assert(evaluate("200+10%", percentError) == 220 && percentError.empty());
        assert(std::abs(evaluate("17%-5", percentError) + 4.83) < 1e-12 && percentError.empty());
        assert(evaluate("not 0", percentError) == 1 && percentError.empty());
        assert(evaluate("!1", percentError) == 0 && percentError.empty());
        assert(evaluate("not not 3", percentError) == 1 && percentError.empty());
        assert(evaluate("-not 0", percentError) == -1 && percentError.empty());
        assert(evaluate("!3!", percentError) == 0 && percentError.empty());
        assert(evaluate("not 0 and 2", percentError) == 1 && percentError.empty());
        engineState().user_args.clear(); engineState().user_arrays.clear(); engineState().user_funcs.clear();
    }
    {
        runScript("print(\"a+=b\")");
        assert(engineState().script_console_output.size() == 1 && engineState().script_console_output[0] == "a+=b");
        runScript("print(\"a\\\"+=b\")");
        assert(engineState().script_console_output.size() == 1 && engineState().script_console_output[0] == "a\"+=b");
        runScript("if(1){ print(1) }else{ print(2) }");
        assert(engineState().script_console_output.size() == 1 && engineState().script_console_output[0] == "1");
        runScript("if(0){ print(1) }else{ print(2) }");
        assert(engineState().script_console_output.size() == 1 && engineState().script_console_output[0] == "2");
        runScript("fn script_once(x) { return x*2 }; print(7)");
        assert(engineState().script_console_output.size() == 1 && engineState().script_console_output[0] == "7");
        runScript("fn sign_test(x) { if(x < 0) { return -1 } else { return 1 } }; a, b = sign_test(-5)");
        auto a = std::find_if(engineState().user_args.begin(), engineState().user_args.end(), [](const UserArg& x) { return x.name == "a"; });
        assert(a != engineState().user_args.end() && a->val == -1);
        runScript("fn loop_test(x) { y=0; for(i=0; i<x; i+=1) { y=y+i }; print(y); return y }; loop_test(4)");
        assert(!engineState().script_console_output.empty() && engineState().script_console_output.front() == "6");
        runScript("fn branch_test(x) { if(x<0) { return -1 } else if(x==0) { return 0 } else { return 1 } }; a,b=branch_test(0)");
        auto branch = std::find_if(engineState().user_args.begin(), engineState().user_args.end(), [](const UserArg& x) { return x.name == "a"; });
        assert(branch != engineState().user_args.end() && branch->val == 0);
        runScript("init_count_test=0; for(init_count_test+=1; init_count_test<3; init_count_test++) {}");
        std::string initError;
        assert(evaluate("init_count_test", initError) == 3 && initError.empty());
        runScript("step_count_test=0; for(i=0; i<2; i++; i==1) { step_count_test+=1 }");
        std::string stepError;
        assert(evaluate("step_count_test", stepError) == 2 && stepError.empty());
        const auto tokenizedHeader = expression::splitArguments("x=mean(1,2);x<3;x++;print(\";\")", ';', 3);
        assert(tokenizedHeader == std::vector<std::string>({"x=mean(1,2)", "x<3", "x++;print(\";\")"}));
        assert(expression::splitArguments(";;", ';', 3) == std::vector<std::string>({"", "", ""}));
        assert(expression::splitArguments("x=(1;0;x++", ';', 3).empty());
        runScript("fn fn_multiple_steps(x) { n=0; for(i=0;i<x;i++;n++) {}; return n }");
        assert(evaluate("fn_multiple_steps (3)", stepError) == 3 && stepError.empty());
        runScript("empty_for_count=0; for(;;) { empty_for_count++; if(empty_for_count==3) { break } }");
        assert(evaluate("empty_for_count", stepError) == 3 && stepError.empty());
        runScript("mixed_loop_count=0; for(i=0;i<3;i++) { j=0; while(j<4) { j++; if(j==2) { continue }; if(j==4) { break }; mixed_loop_count+=1 } }");
        assert(evaluate("mixed_loop_count", stepError) == 6 && stepError.empty());
        assert(evaluate("i", stepError) == 3 && stepError.empty());
        runScript("while(0) { mixed_loop_count=999 }; after_false_while=7");
        assert(evaluate("mixed_loop_count", stepError) == 6 && stepError.empty());
        assert(evaluate("after_false_while", stepError) == 7 && stepError.empty());
        runScript("invalid_for_body=0; for(i=0;i<3) { invalid_for_body=1 }; after_invalid_for=7");
        assert(evaluate("invalid_for_body", stepError) == 0 && stepError.empty());
        assert(evaluate("after_invalid_for", stepError) == 7 && stepError.empty());
        assert(std::any_of(engineState().script_console_output.begin(), engineState().script_console_output.end(),
            [](const std::string& line) { return line.find("Err: Mismatched for syntax") != std::string::npos; }));
        for (const auto& header : {"if(1) {", "while(1) {"}) {
            runScript(std::string("unclosed_block_body=0; ") + header + " unclosed_block_body=1");
            assert(evaluate("unclosed_block_body", stepError) == 0 && stepError.empty());
            assert(std::any_of(engineState().script_console_output.begin(), engineState().script_console_output.end(),
                [](const std::string& line) { return line.find("L:1 Err: Mismatched") != std::string::npos; }));
        }
        for (const auto& header : {"if(1) ignored {", "while(1) ignored {", "for(;;) ignored {"}) {
            runScript(std::string("invalid_header_body=0; ") + header + " invalid_header_body=1 }; after_header_error=7");
            assert(evaluate("invalid_header_body", stepError) == 0 && stepError.empty());
            assert(evaluate("after_header_error", stepError) == 7 && stepError.empty());
            runScript(std::string("fn fn_invalid_block(x) { ") + header + " print(999) }; return x }");
            engineState().script_console_output.clear();
            evaluate("fn_invalid_block (1)", stepError);
            assert(!stepError.empty());
            assert(engineState().script_console_output.empty());
        }
        for (const auto& branch : {"else {", "elif(1) {", "if(0) {} elif(1) ignored {"}) {
            runScript(std::string("invalid_branch_body=0; ") + branch + " invalid_branch_body=1 }; after_branch_error=7");
            assert(evaluate("invalid_branch_body", stepError) == 0 && stepError.empty());
            assert(evaluate("after_branch_error", stepError) == 7 && stepError.empty());
            assert(std::any_of(engineState().script_console_output.begin(), engineState().script_console_output.end(),
                [](const std::string& line) { return line.find("L:1 Err:") != std::string::npos; }));
        }
        runScript("for(i=0; i<2; i=unknown_step_value) { print(i) }");
        assert(std::any_of(engineState().script_console_output.begin(), engineState().script_console_output.end(),
            [](const std::string& line) { return line.find("L:1 Err:") != std::string::npos; }));
        runScript("fn completion_check(x) { return x }");
        assert(std::find(engineState().autocomplete_words.begin(), engineState().autocomplete_words.end(),
            "completion_check(") != engineState().autocomplete_words.end());
        runScript("fn duplicate_params(x,x) { return x }");
        assert(std::none_of(engineState().user_script_funcs.begin(), engineState().user_script_funcs.end(),
            [](const CustomScriptFunc& function) { return function.name == "duplicate_params"; }));
        runScript("fn unfinished_function(x) { return x");
        assert(std::none_of(engineState().user_script_funcs.begin(), engineState().user_script_funcs.end(),
            [](const CustomScriptFunc& function) { return function.name == "unfinished_function"; }));
        assert(std::any_of(engineState().script_console_output.begin(), engineState().script_console_output.end(),
            [](const std::string& line) { return line.find("L:1 Err: Unclosed function body") != std::string::npos; }));
        for (const auto& header : {"fn invalid_header {", "fn invalid_header(x) extra {"}) {
            runScript(std::string(header) + " invalid_body_ran=1 }");
            assert(std::none_of(engineState().user_args.begin(), engineState().user_args.end(),
                [](const UserArg& arg) { return arg.name == "invalid_body_ran"; }));
            assert(std::none_of(engineState().user_script_funcs.begin(), engineState().user_script_funcs.end(),
                [](const CustomScriptFunc& function) { return function.name == "invalid_header"; }));
            assert(std::any_of(engineState().script_console_output.begin(), engineState().script_console_output.end(),
                [](const std::string& line) { return line.find("L:1 Err: Invalid function signature") != std::string::npos; }));
        }
        runScript("step_array=[1]; for(i=0; i<2; i=step_array[99]) {}");
        assert(std::any_of(engineState().script_console_output.begin(), engineState().script_console_output.end(),
            [](const std::string& line) { return line.find("L:1 Err:") != std::string::npos; }));
        runScript("return step_array[99]");
        assert(std::any_of(engineState().script_console_output.begin(), engineState().script_console_output.end(),
            [](const std::string& line) { return line.find("L:1 Err:") != std::string::npos; }));
        runScript("preserved_assignment=42; preserved_assignment=step_array[99]");
        std::string assignmentError;
        assert(evaluate("preserved_assignment", assignmentError) == 42 && assignmentError.empty());
        std::string signatureError;
        bool signatureDefinition = false;
        evaluateInput("invalid-name(x) = x", signatureError, signatureDefinition);
        assert(!signatureError.empty());
        signatureError.clear();
        evaluateInput("invalid_signature(x) extra = x", signatureError, signatureDefinition);
        assert(!signatureError.empty());
        evaluateInput("scaled_parameter(k)=1k+k", signatureError, signatureDefinition);
        assert(signatureError.empty());
        assert(evaluate("scaled_parameter(2)", signatureError) == 1002 && signatureError.empty());
        // Whitespace in calls exercises AST dispatch rather than legacy text expansion.
        assert(evaluate("scaled_parameter (2)", signatureError) == 1002 && signatureError.empty());
        evaluateInput("formula_scope_value=10", signatureError, signatureDefinition);
        evaluateInput("formula_inner(y)=formula_scope_value+y", signatureError, signatureDefinition);
        evaluateInput("formula_outer(formula_scope_value)=formula_inner(2)+formula_scope_value", signatureError, signatureDefinition);
        assert(evaluate("formula_outer (3)", signatureError) == 15 && signatureError.empty());
        assert(evaluate("formula_scope_value", signatureError) == 10 && signatureError.empty());
        evaluateInput("formula_failure(formula_scope_value)=1/(formula_scope_value-3)", signatureError, signatureDefinition);
        evaluate("formula_failure (3)", signatureError);
        assert(signatureError == "Division by Zero");
        assert(evaluate("formula_scope_value", signatureError) == 10 && signatureError.empty());
        evaluateInput("formula_no_args()=42", signatureError, signatureDefinition);
        assert(evaluate("formula_no_args ()+1", signatureError) == 43 && signatureError.empty());
        evaluate("formula_inner ()", signatureError);
        assert(signatureError == "Arg Count Mismatch");
        evaluateInput("formula_unused_argument(x)=42", signatureError, signatureDefinition);
        evaluate("formula_unused_argument (sqrt(-1))", signatureError);
        assert(!signatureError.empty());
        runScript("ast_call_array=[7,9]; fn ast_script_call(x) { return x*2 }");
        assert(evaluate("ast_script_call (3)+ast_call_array[2]", signatureError) == 15 && signatureError.empty());
        assert(evaluate("ast_call_array[1]+ast_script_call (3)+ast_call_array[2]", signatureError) == 22 && signatureError.empty());
        runScript("fn ast_script_global(y) { return formula_scope_value+y }");
        evaluateInput("ast_formula_script(formula_scope_value)=ast_script_global(2)+formula_scope_value", signatureError, signatureDefinition);
        assert(evaluate("ast_formula_script (3)", signatureError) == 15 && signatureError.empty());
        evaluate("ast_script_call ()", signatureError);
        assert(signatureError == "Arg Count Mismatch");
        evaluateInput("ast_reduction=99", signatureError, signatureDefinition);
        assert(evaluate("sum(ast_reduction,1,3,ast_reduction)", signatureError) == 6 && signatureError.empty());
        runScript("fn ast_print_once(x) { print(x); return x }");
        engineState().script_console_output.clear();
        assert(evaluate("if(0,ast_print_once(1),2)", signatureError) == 2 && signatureError.empty());
        assert(engineState().script_console_output.empty());
        assert(evaluate("if(1,2,ast_call_array[99])", signatureError) == 2 && signatureError.empty());
        assert(evaluate("if (1,2,unknown_unselected_name)", signatureError) == 2 && signatureError.empty());
        assert(evaluate("sum (spaced_loop_index,1,3,spaced_loop_index)", signatureError) == 6 && signatureError.empty());
        assert(evaluate("scaled_parameter (2)*50%", signatureError) == 501 && signatureError.empty());
        assert(evaluate("ast_script_call (1)*50%", signatureError) == 1 && signatureError.empty());
        assert(evaluate("if (1,2,1/0)", signatureError) == 2 && signatureError.empty());
        assert(evaluate("if (0,ast_print_once(1),if (1,4,1/0))", signatureError) == 4 && signatureError.empty());
        assert(engineState().script_console_output.empty());
        assert(evaluate("iff (0,ast_print_once(1),1,5,1/0)", signatureError) == 5 && signatureError.empty());
        assert(engineState().script_console_output.empty());
        assert(evaluate("if (1e-13,1,2)", signatureError) == 2 && signatureError.empty());
        assert(evaluate("if (1,ast_print_once(3),ast_print_once(4))", signatureError) == 3 && signatureError.empty());
        assert(engineState().script_console_output.size() == 1);
        engineState().script_console_output.clear();
        std::string nestedConditional = "7";
        for (int depth = 0; depth < 48; ++depth) nestedConditional = "if (1," + nestedConditional + ",1/0)";
        assert(evaluate(nestedConditional, signatureError) == 7 && signatureError.empty());
        std::string longArithmetic = "1";
        for (int term = 0; term < 4096; ++term) longArithmetic += "+1";
        assert(evaluate(longArithmetic, signatureError) == 4097 && signatureError.empty());
        assert(evaluate("iff(0,ast_print_once(1),1,3,ast_call_array[99])", signatureError) == 3 && signatureError.empty());
        assert(engineState().script_console_output.empty());
        assert(evaluate("sum(loop_ast_index,1,3,ast_print_once(loop_ast_index))", signatureError) == 6 && signatureError.empty());
        assert(engineState().script_console_output.size() == 3);
        engineState().script_console_output.clear();
        evaluateInput("scoped_reduction(x)=sum(x,1,3,x)+x", signatureError, signatureDefinition);
        assert(evaluate("scoped_reduction(10)", signatureError) == 16 && signatureError.empty());
        evaluateInput("inherited_reduction(x)=sum(local_idx,1,3,x*local_idx)", signatureError, signatureDefinition);
        assert(evaluate("inherited_reduction(10)", signatureError) == 60 && signatureError.empty());
        engineState().user_consts.push_back({"reduction_constant", 99});
        assert(evaluate("sum(reduction_constant,1,3,reduction_constant)", signatureError) == 6 && signatureError.empty());
        assert(evaluate("reduction_constant", signatureError) == 99 && signatureError.empty());
        assert(evaluate("sum(reduction_constant,1,2,sum(reduction_constant,1,3,reduction_constant)+reduction_constant)", signatureError) == 15 && signatureError.empty());
        assert(evaluate("reduction_constant", signatureError) == 99 && signatureError.empty());
        evaluate("sum(reduction_constant,1,3,1/(reduction_constant-2))", signatureError);
        assert(!signatureError.empty());
        assert(evaluate("reduction_constant", signatureError) == 99 && signatureError.empty());
        evaluate("ast_print_once(3)+unknown_ast_name", signatureError);
        assert(!signatureError.empty());
        assert(engineState().script_console_output.size() == 1);
        assert(evaluate("ast_call_array[ast_script_call (1)]*50%", signatureError) == 4.5 && signatureError.empty());
        evaluateInput("ast_shadow_array(ast_call_array)=sum(ast_call_array)*50%", signatureError, signatureDefinition);
        const double shadowResult = evaluate("ast_shadow_array (2)", signatureError);
        assert(shadowResult == 1 && signatureError.empty());
        runScript("fn fn_header_array(x) { local_header_array=[0]; for(local_header_array[1]=0; local_header_array[1]<x; local_header_array[1]=local_header_array[1]+1) {}; return local_header_array[1] }");
        assert(evaluate("fn_header_array (3)", signatureError) == 3 && signatureError.empty());
        assert(engineState().user_arrays.find("local_header_array") == engineState().user_arrays.end());
        engineState().user_consts.push_back({"fn_header_constant", 7});
        runScript("fn fn_header_const_check(x) { for(fn_header_constant=0; 0; x++) {}; return x }");
        evaluate("fn_header_const_check (3)", signatureError);
        assert(signatureError == "Const Error");
        assert(evaluate("fn_header_constant", signatureError) == 7 && signatureError.empty());
        assert(!executeScriptAssignments("pi=0", signatureError));
        assert(signatureError == "Reserved name: pi");
        runScript("fn fn_tab_return(x) { return\tx+1 }");
        assert(evaluate("fn_tab_return (2)", signatureError) == 3 && signatureError.empty());
        engineState().script_console_output.clear();
        bool returnArray = false;
        const auto scalarReturn = evaluateScriptReturnValues("sum([ast_print_once(1),2])", returnArray, signatureError);
        assert(scalarReturn == std::vector<double>({3}) && !returnArray && signatureError.empty());
        assert(engineState().script_console_output.size() == 1);
        evaluateScriptReturnValues("1,", returnArray, signatureError);
        assert(signatureError == "Empty return value");
        runScript("tuple_probe_a,tuple_probe_b=1,2");
        assert(evaluate("tuple_probe_a", signatureError) == 1 && signatureError.empty());
        assert(evaluate("tuple_probe_b", signatureError) == 2 && signatureError.empty());
        runScript("tuple_probe_a,tuple_probe_b=sum([ast_print_once(1),2])");
        assert(engineState().script_console_output.size() == 1);
        assert(evaluate("tuple_probe_a", signatureError) == 3 && signatureError.empty());
        assert(evaluate("tuple_probe_b", signatureError) == 0 && signatureError.empty());
        runScript("assignment_preserved=[1,2]; assignment_preserved=unknown_assignment_value");
        assert(engineState().user_arrays.at("assignment_preserved") == std::vector<double>({1,2}));
        runScript("assignment_preserved=sum(assignment_preserved)");
        assert(evaluate("assignment_preserved", signatureError) == 3 && signatureError.empty());
        assert(engineState().user_arrays.count("assignment_preserved") == 0);
        runScript("script_map_source=[1,4,9]; script_map_source=sqrt(script_map_source)");
        assert(engineState().user_arrays.at("script_map_source") == std::vector<double>({1,2,3}));
        runScript("script_map_formula(x)=x*x; script_map_result=script_map_formula(script_map_source)");
        assert(engineState().user_arrays.at("script_map_result") == std::vector<double>({1,4,9}));
        runScript("assignment_once=sum([ast_print_once(1),2])");
        assert(evaluate("assignment_once", signatureError) == 3 && signatureError.empty());
        assert(engineState().script_console_output.size() == 1);
        runScript("array_assignment_once=[ast_print_once(1)]+[ast_print_once(2)]");
        assert(engineState().user_arrays.at("array_assignment_once") == std::vector<double>({3}));
        assert(engineState().script_console_output.size() == 2);
        runScript("array_scalar_once=[1,2]*sum([ast_print_once(1),2])");
        assert(engineState().user_arrays.at("array_scalar_once") == std::vector<double>({3,6}));
        assert(engineState().script_console_output.size() == 1);
        runScript("scalar_array_once=sum([ast_print_once(1),2])*[1,2]");
        assert(engineState().user_arrays.at("scalar_array_once") == std::vector<double>({3,6}));
        assert(engineState().script_console_output.size() == 1);
        runScript("empty_array_result=[]+[]");
        assert(engineState().user_arrays.count("empty_array_result") == 1);
        assert(engineState().user_arrays.at("empty_array_result").empty());
        engineState().user_arrays["ast_statistics"] = {1,2,2,5};
        const std::vector<std::pair<std::string, double>> statistics = {
            {"sum",10}, {"prod",20}, {"mean",2.5}, {"min",1}, {"max",5},
            {"median",2}, {"mode",2}, {"var",3}, {"std",std::sqrt(3.0)}
        };
        for (const auto& [name, expected] : statistics) {
            const auto result = expression::ArithmeticExpression::evaluate(name + " (ast_statistics)", signatureError,
                lookupScalar, lookupUnary, lookupArray, lookupBinary, lookupVariadic, lookupCustomFunction);
            assert(result && std::abs(*result - expected) < 1e-12 && signatureError.empty());
            assert(std::abs(evaluate(name + "([1,2,2,5])", signatureError) - expected) < 1e-12 && signatureError.empty());
            assert(evaluate(name + "(empty_array_result)", signatureError) == 0 && signatureError.empty());
        }
        assert(engineState().user_arrays.at("ast_statistics") == std::vector<double>({1,2,2,5}));
        assert(evaluate("ast_script_call(1)+sum(ast_statistics)", signatureError) == 12 && signatureError.empty());
        engineState().user_arrays["ast_dot_right"] = {2,3,4,5};
        const auto astDot = expression::ArithmeticExpression::evaluate("dot (ast_statistics,ast_dot_right)", signatureError,
            lookupScalar, lookupUnary, lookupArray, lookupBinary, lookupVariadic, lookupCustomFunction);
        assert(astDot && *astDot == 41 && signatureError.empty());
        assert(evaluate("dot([1,2,2,5],[2,3,4,5])", signatureError) == 41 && signatureError.empty());
        assert(evaluate("dot(empty_array_result,empty_array_result)", signatureError) == 0 && signatureError.empty());
        assert(evaluate("dot([],[])", signatureError) == 0 && signatureError.empty());
        evaluate("dot(ast_statistics,empty_array_result)", signatureError);
        assert(signatureError == "Array size mismatch (4 vs 0)");
        assert(evaluate("dot(2,3)", signatureError) == 6 && signatureError.empty());
        assert(evaluate("ast_script_call(1)+dot(ast_statistics,ast_dot_right)", signatureError) == 43 && signatureError.empty());
        runScript("tuple_probe_a,tuple_probe_b=1,");
        assert(evaluate("tuple_probe_a", signatureError) == 3 && signatureError.empty());
        assert(evaluate("tuple_probe_b", signatureError) == 0 && signatureError.empty());
        runScript("for(invalid-loop-name=0; 0; i++) {}");
        assert(std::none_of(engineState().user_args.begin(), engineState().user_args.end(),
            [](const UserArg& arg) { return arg.name == "invalid-loop-name"; }));
        runScript("invalid-increment-name++");
        assert(std::none_of(engineState().user_args.begin(), engineState().user_args.end(),
            [](const UserArg& arg) { return arg.name == "invalid-increment-name"; }));
        runScript("tuple_preserved=7; tuple_preserved,tuple_preserved=1,2");
        std::string tupleError;
        assert(evaluate("tuple_preserved", tupleError) == 7 && tupleError.empty());
        engineState().user_consts.push_back({"loop_constant_test", 7});
        runScript("for(loop_constant_test=0; 0; i++) {}");
        assert(std::none_of(engineState().user_args.begin(), engineState().user_args.end(),
            [](const UserArg& arg) { return arg.name == "loop_constant_test"; }));
        runScript("loop_constant_test++");
        assert(std::none_of(engineState().user_args.begin(), engineState().user_args.end(),
            [](const UserArg& arg) { return arg.name == "loop_constant_test"; }));
        runScript("for_array=[0]; for(i=0; i<2; for_array[1]=for_array[1]+1) { i++ }");
        assert(engineState().user_arrays.at("for_array")[0] == 2);
        runScript("loop_type_test=[1,2]; for(loop_type_test=3; 0; i++) {}");
        assert(engineState().user_arrays.count("loop_type_test") == 0);
        std::string loopTypeError;
        assert(evaluate("loop_type_test", loopTypeError) == 3 && loopTypeError.empty());
        runScript("prefix_test=5; ++prefix_test; --prefix_test; ++prefix_test");
        std::string prefixError;
        assert(evaluate("prefix_test", prefixError) == 6 && prefixError.empty());
        runScript("fn empty_argument_test(x) { return x }");
        std::string emptyArgumentError;
        evaluate("empty_argument_test(,1)", emptyArgumentError);
        assert(!emptyArgumentError.empty());
        assert(preprocessFactorial("\"3!\"") == "\"3!\"");
        assert(preprocessPercentage("\"50%\"") == "\"50%\"");
        assert(preprocessLen("\"len(missing_array)\"") == "\"len(missing_array)\"");
        assert(expression::callOffset("\"f(1)\" + f(2)", "f") == 9);
        const std::string spacedCall = "\"f (1)\" + f \t(2)";
        const auto callSite = expression::findCall(spacedCall, "f");
        assert(callSite && callSite->nameOffset == spacedCall.find("f \t") && callSite->openOffset == spacedCall.rfind('('));
        assert(!expression::findCall("different_f (2)", "f"));
        assert(preprocessLen("len ([1,2,3])") == "3");
        assert(preprocessVectorStats("dot ([1,2], [3,4])") == "11");
        assert(preprocessLen("\"len (x)\"") == "\"len (x)\"");
        assert(expression::matchingGroupEnd("f(\")\", [1,2])", 1) == 12);
        runScript("return 1, unknown_return_value");
        assert(std::any_of(engineState().script_console_output.begin(), engineState().script_console_output.end(),
            [](const std::string& line) { return line.find("L:1 Err:") != std::string::npos; }));
        runScript("fn pair_test(x,y) { return x,y }; a,b = pair_test(1)");
        assert(!engineState().script_console_output.empty() && engineState().script_console_output.back().find("Arg Count Mismatch") != std::string::npos);
        runScript("fn pair_test(x,y) { return x,y }; a,b = pair_test(1,unknown_value)");
        assert(!engineState().script_console_output.empty() && engineState().script_console_output.back().find("Err:") != std::string::npos);
        runScript("fn recursive_script(x) { return recursive_script(x) }; recursive_script(1)");
        assert(!engineState().script_console_output.empty() && engineState().script_console_output.back().find("recursion limit") != std::string::npos);
        runScript("for (i=0; i<5; i+=1) { if(i==1) { continue }; if(i==3) { break }; print(i) }");
        assert(engineState().script_console_output.size() == 2 && engineState().script_console_output[0] == "0" && engineState().script_console_output[1] == "2");
        runScript("counter=0; for (i=0; i<4000; i+=1) { counter=counter+1; counter=counter+1; counter=counter+1 }; print(counter)");
        assert(!engineState().script_console_output.empty() && engineState().script_console_output.back() == "12000");
        engineState().user_consts.push_back({"constant_test", 8});
        runScript("constant_test, b = [1,2]");
        assert(engineState().user_consts.back().val == 8 && !engineState().script_console_output.empty());
        engineState().user_consts.clear();
        engineState().user_script_funcs.clear();
        engineState().user_args.clear(); engineState().user_arrays.clear();
    }
    engineState().fmt_mode = 4; engineState().fmt_precision = 2;
    assert(fmtNum(999.999) == "1.00e+03");
    engineState().fmt_mode = 0; engineState().fmt_precision = 6;
    for (const auto& expression : {"gcd(1e30,2)", "lcm(4000000000,4000000001)", "sum(i,-5e18,5e18,1)", "sum(i,1,400,sum(j,1,400,1))"}) {
        std::string error;
        evaluate(expression, error);
        if (error.empty()) { std::cerr << "Expected error: " << expression << std::endl; assert(false); }
    }
    {
        std::string error; bool isArray = false;
        parseArrayExpr("0:1:1e10", isArray, error);
        assert(!error.empty());
        bool isDefinition = false;
        evaluateInput("oversized=[0:1:1e10]", error, isDefinition); assert(!error.empty());
        evaluateInput("a=[1,2,3]", error, isDefinition); assert(error.empty());
        evaluateInput("f(x)=x^2", error, isDefinition); assert(error.empty());
        auto values = parseArrayExpr("f(a)", isArray, error);
        assert(error.empty() && isArray && values.size() == 3 && values[1] == 4);
        evaluateInput("b=f(a)", error, isDefinition); assert(error.empty());
        expectNumber("b[2]", 4);
        std::string longArrayExpression = "a";
        for (int i = 0; i < 30; ++i) longArrayExpression += "+a";
        evaluateInput("many=" + longArrayExpression, error, isDefinition); assert(error.empty());
        expectNumber("many[2]", 62);
        parseArrayExpr("foo(a)", isArray, error); assert(!error.empty());
        engineState().user_arrays.clear(); engineState().user_funcs.clear();
        for (const auto& expression : {"pi=3", "sin=2", "sqrt(x)=5", "pow=3", "f(pi)=2"}) {
            evaluateInput(expression, error, isDefinition); assert(!error.empty());
        }
        runScript("for (sin=1; sin<2; sin++) { print(sin) }");
        assert(!engineState().script_console_output.empty() && engineState().script_console_output.front().find("Reserved name") != std::string::npos);
        runScript("print(\"sin=2\")");
        assert(!engineState().script_console_output.empty() && engineState().script_console_output.front().find("Reserved name") == std::string::npos);
        evaluateInput("g(x)=x*2", error, isDefinition); assert(error.empty());
        expectNumber("log(100)", 2);
        evaluateInput("n(x)=x+1", error, isDefinition); assert(error.empty());
        expectNumber("sin(0)", 0);
        evaluateInput("d(x,y)=x-y", error, isDefinition); assert(error.empty());
        engineState().user_args.push_back({"x",2}); engineState().user_args.push_back({"y",5});
        expectNumber("d(y,x)", 3);
        expectNumber("g(1)+g(1)+g(1)+g(1)+g(1)+g(1)+g(1)+g(1)+g(1)+g(1)+g(1)", 22);
        engineState().user_args.clear(); engineState().user_funcs.clear();
    }

    for (const auto& tc : std::vector<std::pair<std::string, double>>{
             {"3!+4!", 30}, {"(2+1)!*2!", 12}, {"3!!", 720},
             {"3! != 4!", 1}, {"2 xor 3", 0}, {"1 xor 1", 0},
             {"0 xor 0", 0}, {"0 xor 3", 1}, {"1 xor 0 and 0", 1}}) {
        std::string testError;
        double actual = evaluate(tc.first, testError);
        if (!testError.empty() || actual != tc.second) {
            std::cerr << "Expression failed: " << tc.first << " = " << actual << " " << testError << std::endl;
            assert(false);
        }
    }

    // 3. Units adapter test (conversion to explicit target unit)
    //    The path is resolved relative to the binary's working dir.
    std::string units_dat = std::string(UNITS_DAT_PATH);
    ConvResult c1 = handleConv(5, "mile", "km", units_dat);
    std::cout << "conv(5, \"mile\", \"km\") = " << c1.value << " " << c1.unitStr << std::endl;
    assert(c1.success);
    assert(std::abs(c1.value - 8.04672) < 0.0001);
    assert(c1.unitStr == "km");

    // GNU Units 2.27 loads definitions from adjacent included data files.
    assert(bridge_init(units_dat.c_str()) == 0);
    double included_value = 0.0;
    char included_unit[256] = {}, included_error[256] = {};
    assert(bridge_convert(1.0, "BTC", "bitcoin", &included_value,
                          included_unit, sizeof(included_unit), included_error,
                          sizeof(included_error)) == 0);
    assert(std::abs(included_value - 1.0) < 1e-9);
    std::string long_unit(256, 'm');
    assert(bridge_convert(1.0, long_unit.c_str(), "m", &included_value,
                          included_unit, sizeof(included_unit), included_error,
                          sizeof(included_error)) != 0);
    assert(std::string(included_error).find("too long") != std::string::npos);

    // 4. Units adapter test (conversion to SI base units)
    //    Note: GNU Units uses "hr" for hour, "h" means hectare.
    ConvResult c2 = handleConv(100, "km/hr", "", units_dat);
    std::cout << "conv(100, \"km/hr\") = " << c2.value << " " << c2.unitStr << std::endl;
    assert(c2.success);
    assert(std::abs(c2.value - 27.7778) < 0.001);

    // 5. Units adapter test (incompatible units validation error)
    ConvResult c3 = handleConv(5, "mile", "deg", units_dat);
    std::cout << "conv(5, \"mile\", \"deg\") error = " << c3.errorMsg << std::endl;
    assert(!c3.success);
    assert(c3.errorMsg.find("Incompatible") != std::string::npos);

    // 6. Custom dB & AWG tests
    ConvResult c_db1 = handleConv(30, "dBm", "mW", units_dat);
    std::cout << "conv(30, \"dBm\", \"mW\") = " << c_db1.value << " " << c_db1.unitStr << std::endl;
    assert(c_db1.success && std::abs(c_db1.value - 1000.0) < 0.001);

    ConvResult c_db2 = handleConv(20, "dB", "times", units_dat);
    std::cout << "conv(20, \"dB\", \"times\") = " << c_db2.value << " " << c_db2.unitStr << std::endl;
    assert(c_db2.success && std::abs(c_db2.value - 100.0) < 0.001);

    ConvResult c_db3 = handleConv(20, "dB_v", "times", units_dat);
    std::cout << "conv(20, \"dB_v\", \"times\") = " << c_db3.value << " " << c_db3.unitStr << std::endl;
    assert(c_db3.success && std::abs(c_db3.value - 10.0) < 0.001);

    ConvResult c_awg1 = handleConv(24, "AWG", "mm^2", units_dat);
    std::cout << "conv(24, \"AWG\", \"mm^2\") = " << c_awg1.value << " " << c_awg1.unitStr << std::endl;
    assert(c_awg1.success && std::abs(c_awg1.value - 0.2047) < 0.005 && c_awg1.unitStr == "mm^2");

    ConvResult c_awg2 = handleConv(0.205, "mm^2", "AWG", units_dat);
    std::cout << "conv(0.205, \"mm^2\", \"AWG\") = " << c_awg2.value << " " << c_awg2.unitStr << std::endl;
    assert(c_awg2.success && std::abs(c_awg2.value - 24.0) < 0.1);

    // Standard cross-section (std_mm2) & Standard AWG (std_awg) tests with copper safety margin
    ConvResult c_std_mm1 = handleConv(2.1, "mm2", "std_mm2", units_dat);
    std::cout << "conv(2.1, \"mm2\", \"std_mm2\") = " << c_std_mm1.value << " " << c_std_mm1.unitStr << std::endl;
    assert(c_std_mm1.success && c_std_mm1.value == 2.5 && c_std_mm1.unitStr == "std_mm2");

    ConvResult c_std_mm2 = handleConv(2.5, "mm^2", "std_mm2", units_dat);
    std::cout << "conv(2.5, \"mm^2\", \"std_mm2\") = " << c_std_mm2.value << " " << c_std_mm2.unitStr << std::endl;
    assert(c_std_mm2.success && c_std_mm2.value == 2.5);

    ConvResult c_std_mm3 = handleConv(14, "AWG", "std_mm2", units_dat); // 14 AWG ≈ 2.08 mm^2 -> 2.5 mm^2
    std::cout << "conv(14, \"AWG\", \"std_mm2\") = " << c_std_mm3.value << " " << c_std_mm3.unitStr << std::endl;
    assert(c_std_mm3.success && c_std_mm3.value == 2.5);

    ConvResult c_std_awg1 = handleConv(2.5, "mm2", "std_awg", units_dat); // 2.5 mm^2 is between 13 and 14 AWG -> 13 AWG for safety
    std::cout << "conv(2.5, \"mm2\", \"std_awg\") = " << c_std_awg1.value << " " << c_std_awg1.unitStr << std::endl;
    assert(c_std_awg1.success && c_std_awg1.value == 13.0 && c_std_awg1.unitStr == "std_awg");

    ConvResult c_std_awg2 = handleConv(2.08, "mm^2", "std_awg", units_dat); // 2.08 mm^2 -> 14 AWG
    std::cout << "conv(2.08, \"mm^2\", \"std_awg\") = " << c_std_awg2.value << " " << c_std_awg2.unitStr << std::endl;
    assert(c_std_awg2.success && c_std_awg2.value == 14.0);

    // Resistor / Capacitor / Inductor Code tests
    ConvResult c_res1 = handleConv(0, "01C", "ohm", units_dat);
    std::cout << "conv(\"01C\", \"ohm\") = " << c_res1.value << " " << c_res1.unitStr << std::endl;
    assert(c_res1.success && std::abs(c_res1.value - 10000.0) < 0.001);

    ConvResult c_res2 = handleConv(10000, "ohm", "eia96", units_dat);
    std::cout << "conv(10000, \"eia96\") = " << c_res2.unitStr << std::endl;
    assert(c_res2.success && c_res2.unitStr == "01C");

    ConvResult c_res3 = handleConv(0, "472", "smd4", units_dat);
    std::cout << "conv(\"472\", \"smd4\") = " << c_res3.unitStr << std::endl;
    assert(c_res3.success && (c_res3.unitStr == "4701" || c_res3.unitStr == "4720"));

    // conv(200, "smd3") test
    ConvResult c_res4 = handleConv(200, "smd3", "", units_dat);
    if (!c_res4.success) c_res4 = handleConv(0, "200", "smd3", units_dat);
    std::cout << "conv(200, \"smd3\") = " << c_res4.unitStr << std::endl;
    assert(c_res4.success && (c_res4.unitStr == "201" || c_res4.unitStr == "200"));

    // Capacitor code: "104" -> 0.1 uF / 100 nF
    ConvResult c_cap1 = handleConv(0, "104", "uf", units_dat);
    std::cout << "conv(\"104\", \"uf\") = " << c_cap1.value << " " << c_cap1.unitStr << std::endl;
    assert(c_cap1.success && std::abs(c_cap1.value - 0.1) < 0.0001);

    // Inductor code: "4R7" -> 4.7 uH
    ConvResult c_ind1 = handleConv(0, "4R7", "uh", units_dat);
    std::cout << "conv(\"4R7\", \"uh\") = " << c_ind1.value << " " << c_ind1.unitStr << std::endl;
    assert(c_ind1.success && std::abs(c_ind1.value - 4.7) < 0.0001);

    // R-notation tests: 2R2 -> 2.2 Ohm, R2 -> 0.2 Ohm, R47 -> 0.47 Ohm
    ConvResult c_r1 = handleConv(0, "2R2", "ohm", units_dat);
    std::cout << "conv(\"2R2\", \"ohm\") = " << c_r1.value << " " << c_r1.unitStr << std::endl;
    assert(c_r1.success && std::abs(c_r1.value - 2.2) < 0.001);

    ConvResult c_r2 = handleConv(0, "R2", "ohm", units_dat);
    std::cout << "conv(\"R2\", \"ohm\") = " << c_r2.value << " " << c_r2.unitStr << std::endl;
    assert(c_r2.success && std::abs(c_r2.value - 0.2) < 0.001);

    ConvResult c_r3 = handleConv(0, "R47", "ohm", units_dat);
    std::cout << "conv(\"R47\", \"ohm\") = " << c_r3.value << " " << c_r3.unitStr << std::endl;
    assert(c_r3.success && std::abs(c_r3.value - 0.47) < 0.001);

    // EIA-96 Alternative Multiplier Letters: Z (0.001), Y/R (0.01), X/S (0.1), A (1), B/H (10), C (100), D (1000), E (10000), F (100000)
    ConvResult c_eia_z = handleConv(0, "01Z", "ohm", units_dat);
    assert(c_eia_z.success && std::abs(c_eia_z.value - 0.1) < 0.001);

    ConvResult c_eia_y = handleConv(0, "01Y", "ohm", units_dat);
    assert(c_eia_y.success && std::abs(c_eia_y.value - 1.0) < 0.001);

    ConvResult c_eia_r = handleConv(0, "01R", "ohm", units_dat);
    assert(c_eia_r.success && std::abs(c_eia_r.value - 1.0) < 0.001);

    ConvResult c_eia_x = handleConv(0, "01X", "ohm", units_dat);
    assert(c_eia_x.success && std::abs(c_eia_x.value - 10.0) < 0.001);

    ConvResult c_eia_s = handleConv(0, "01S", "ohm", units_dat);
    assert(c_eia_s.success && std::abs(c_eia_s.value - 10.0) < 0.001);

    ConvResult c_eia_a = handleConv(0, "01A", "ohm", units_dat);
    assert(c_eia_a.success && std::abs(c_eia_a.value - 100.0) < 0.001);

    ConvResult c_eia_b = handleConv(0, "01B", "ohm", units_dat);
    assert(c_eia_b.success && std::abs(c_eia_b.value - 1000.0) < 0.001);

    ConvResult c_eia_h = handleConv(0, "01H", "ohm", units_dat);
    assert(c_eia_h.success && std::abs(c_eia_h.value - 1000.0) < 0.001);

    ConvResult c_eia_d = handleConv(0, "01D", "ohm", units_dat);
    assert(c_eia_d.success && std::abs(c_eia_d.value - 100000.0) < 0.001);

    ConvResult c_eia_e = handleConv(0, "01E", "ohm", units_dat);
    assert(c_eia_e.success && std::abs(c_eia_e.value - 1000000.0) < 0.001);

    ConvResult c_eia_f = handleConv(0, "01F", "ohm", units_dat);
    assert(c_eia_f.success && std::abs(c_eia_f.value - 10000000.0) < 0.001);

    // 7. Format tests (NORM, FLT, FIX, SCI, ENG, digit grouping for int & frac)
    engineState().use_thousands_sep = true;

    // NORM mode (Mode 0)
    engineState().fmt_mode = 0;
    engineState().fmt_precision = 6;
    std::string s_norm_std = fmtNum(1234.567);
    std::cout << "NORM std: " << s_norm_std << std::endl;
    assert(s_norm_std == "1 234.57");

    std::string s_norm_large = fmtNum(12345678.9); // >= 1e7 => sci
    std::cout << "NORM large: " << s_norm_large << std::endl;
    assert(s_norm_large.find("e+") != std::string::npos);

    // FLT mode (Mode 1: Pure float, no exponent, strips trailing zeros)
    engineState().fmt_mode = 1;
    engineState().fmt_precision = 6;
    std::string s_flt = fmtNum(1234.500);
    std::cout << "FLT 6dp + spaces: " << s_flt << std::endl;
    assert(s_flt == "1 234.5");

    std::string s_flt_small = fmtNum(0.0000012345);
    std::cout << "FLT small: " << s_flt_small << std::endl;
    assert(s_flt_small == "0.000 001");

    // FIX mode (Mode 2: Fixed point, preserves trailing zeros)
    engineState().fmt_mode = 2;
    engineState().fmt_precision = 6;
    std::string s_fix = fmtNum(1234567.8901234);
    std::cout << "FIX 6dp + spaces: " << s_fix << std::endl;
    assert(s_fix == "1 234 567.890 123");

    std::string s_fix_zeros = fmtNum(12.34);
    std::cout << "FIX 6dp zeros: " << s_fix_zeros << std::endl;
    assert(s_fix_zeros == "12.340 000");

    std::string s_frac = fmtNum(0.0000012345);
    std::cout << "FIX 6dp + spaces frac: " << s_frac << std::endl;
    assert(s_frac == "0.000 001");

    // SCI mode (Mode 3: Scientific)
    engineState().fmt_mode = 3;
    engineState().fmt_precision = 4;
    std::string s_sci = fmtNum(0.0000012345);
    std::cout << "SCI 4dp + spaces: " << s_sci << std::endl;
    assert(s_sci == "1.234 5e-06");

    // ENG mode (Mode 4: Engineering exponent multiple of 3)
    engineState().fmt_mode = 4;
    engineState().fmt_precision = 3;
    std::string s_eng1 = fmtNum(123456.789);
    std::cout << "ENG 3dp + spaces (123k): " << s_eng1 << std::endl;
    assert(s_eng1 == "123.457e+03");

    std::string s_eng2 = fmtNum(0.0000012345);
    std::cout << "ENG 3dp + spaces (u): " << s_eng2 << std::endl;
    assert(s_eng2 == "1.235e-06");

    // Test without digit grouping
    engineState().use_thousands_sep = false;
    engineState().fmt_mode = 2; // FIX
    engineState().fmt_precision = 6;
    std::string s_nogroup = fmtNum(0.0000012345);
    std::cout << "FIX 6dp no spaces: " << s_nogroup << std::endl;
    assert(s_nogroup == "0.000001");

    // --- Comprehensive TAB Autocomplete Test Suite ---
    auto reset_tab = []() {
        std::string empty = "";
        int rpos = -999;
        handleTabCompletion(empty, rpos);
    };

    // Test Case 1: Prefix 'p' cycling with auto_brackets = true
    reset_tab();
    engineState().auto_brackets = true;
    std::string expr1 = "p";
    int cursor1 = 1;

    handleTabCompletion(expr1, cursor1);
    std::cout << "Test 1.1 [p -> pi]: " << expr1 << " cursor=" << cursor1 << std::endl;
    assert(expr1 == "pi" && cursor1 == 2);

    handleTabCompletion(expr1, cursor1);
    std::cout << "Test 1.2 [pi -> phi]: " << expr1 << " cursor=" << cursor1 << std::endl;
    assert(expr1 == "phi" && cursor1 == 3);

    handleTabCompletion(expr1, cursor1);
    std::cout << "Test 1.3 [phi -> print()]: " << expr1 << " cursor=" << cursor1 << std::endl;
    assert(expr1 == "print()" && cursor1 == 6); // comp_start(0) + "print(".length(6)

    handleTabCompletion(expr1, cursor1);
    std::cout << "Test 1.4 [print() -> plot()]: " << expr1 << " cursor=" << cursor1 << std::endl;
    assert(expr1 == "plot()" && cursor1 == 5); // comp_start(0) + "plot(".length(5)

    handleTabCompletion(expr1, cursor1);
    std::cout << "Test 1.5 [plot() -> plot.show()]: " << expr1 << " cursor=" << cursor1 << std::endl;
    assert(expr1 == "plot.show()" && cursor1 == 11);

    handleTabCompletion(expr1, cursor1);
    std::cout << "Test 1.6 [plot.show() -> plot.close()]: " << expr1 << " cursor=" << cursor1 << std::endl;
    assert(expr1 == "plot.close()" && cursor1 == 12);

    // Test Case 2: Autocomplete inside existing parens (e.g. "sin(p|)")
    reset_tab();
    std::string expr2 = "sin(p)";
    int cursor2 = 5; // cursor right after 'p', before ')'

    handleTabCompletion(expr2, cursor2);
    std::cout << "Test 2.1 [sin(p|) -> sin(pi)]: " << expr2 << " cursor=" << cursor2 << std::endl;
    assert((expr2 == "sin(pi)" || expr2 == "sin(pi))") && (cursor2 == 6 || cursor2 == 7));

    // Test Case 3: Prefix 's' (sin(), sinh(), sqrt(), sgn(), std())
    reset_tab();
    std::string expr3 = "s";
    int cursor3 = 1;
    handleTabCompletion(expr3, cursor3);
    std::cout << "Test 3.1 [s -> sin()]: " << expr3 << " cursor=" << cursor3 << std::endl;
    assert(expr3 == "sin()" && cursor3 == 4);

    handleTabCompletion(expr3, cursor3);
    std::cout << "Test 3.2 [sin() -> sinh()]: " << expr3 << " cursor=" << cursor3 << std::endl;
    assert(expr3 == "sinh()" && cursor3 == 5);

    // Test Case 4: Prefix 'c' cycling (cos(), ctan(), cosh(), cbrt(), ceil(), conv())
    reset_tab();
    std::string expr4 = "c";
    int cursor4 = 1;
    handleTabCompletion(expr4, cursor4);
    std::cout << "Test 4.1 [c -> cos()]: " << expr4 << " cursor=" << cursor4 << std::endl;
    assert(expr4 == "cos()" && cursor4 == 4);

    handleTabCompletion(expr4, cursor4);
    std::cout << "Test 4.2 [cos() -> ctan()]: " << expr4 << " cursor=" << cursor4 << std::endl;
    assert(expr4 == "ctan()" && cursor4 == 5);

    handleTabCompletion(expr4, cursor4);
    std::cout << "Test 4.2b [ctan() -> cot()]: " << expr4 << " cursor=" << cursor4 << std::endl;
    assert(expr4 == "cot()" && cursor4 == 4);

    handleTabCompletion(expr4, cursor4);
    std::cout << "Test 4.3 [cot() -> cosh()]: " << expr4 << " cursor=" << cursor4 << std::endl;
    assert(expr4 == "cosh()" && cursor4 == 5);

    handleTabCompletion(expr4, cursor4);
    std::cout << "Test 4.4 [cosh() -> cbrt()]: " << expr4 << " cursor=" << cursor4 << std::endl;
    assert(expr4 == "cbrt()" && cursor4 == 5);

    handleTabCompletion(expr4, cursor4);
    std::cout << "Test 4.5 [cbrt() -> ceil()]: " << expr4 << " cursor=" << cursor4 << std::endl;
    assert(expr4 == "ceil()" && cursor4 == 5);

    handleTabCompletion(expr4, cursor4);
    std::cout << "Test 4.6 [ceil() -> conv()]: " << expr4 << " cursor=" << cursor4 << std::endl;
    assert(expr4 == "conv()" && cursor4 == 5);

    handleTabCompletion(expr4, cursor4);
    std::cout << "Test 4.7 [conv() -> const ]: " << expr4 << " cursor=" << cursor4 << std::endl;
    assert(expr4 == "const " && cursor4 == 6);

    // Test Case 4.7: Upper 'C' prefix matches C()
    reset_tab();
    std::string expr4_cap = "C";
    int cursor4_cap = 1;
    handleTabCompletion(expr4_cap, cursor4_cap);
    std::cout << "Test 4.7 [C -> C()]: " << expr4_cap << " cursor=" << cursor4_cap << std::endl;
    assert(expr4_cap == "C()" && cursor4_cap == 2);

    engineState().user_script_funcs.clear();
    engineState().autocomplete_words.erase(std::remove(engineState().autocomplete_words.begin(), engineState().autocomplete_words.end(), "pair_test("), engineState().autocomplete_words.end());

    // Test Case 5: User variable & constant priority (cycling "para" -> "power")
    reset_tab();
    engineState().user_args.clear();
    engineState().user_consts.clear();
    engineState().user_consts.push_back({"para", 3.14});
    engineState().user_args.push_back({"power", 42.0});

    std::string expr5 = "pa";
    int cursor5 = 2;
    handleTabCompletion(expr5, cursor5);
    std::cout << "Test 5.1 [pa -> para]: " << expr5 << " cursor=" << cursor5 << std::endl;
    assert(expr5 == "para" && cursor5 == 4);

    handleTabCompletion(expr5, cursor5);
    std::cout << "Test 5.2 [repeat TAB on single match para -> para]: " << expr5 << " cursor=" << cursor5 << std::endl;
    assert(expr5 == "para" && cursor5 == 4);

    // Clear user variables for clean subsequent tests
    engineState().user_args.clear();
    engineState().user_consts.clear();

    // Test Case 6: Autocomplete with auto_brackets = false
    reset_tab();
    engineState().auto_brackets = false;
    std::string expr6 = "p";
    int cursor6 = 1;
    handleTabCompletion(expr6, cursor6);
    std::cout << "Test 6.1 [no auto_brackets p -> pi]: " << expr6 << " cursor=" << cursor6 << std::endl;
    assert(expr6 == "pi" && cursor6 == 2);

    handleTabCompletion(expr6, cursor6);
    std::cout << "Test 6.2 [no auto_brackets pi -> phi]: " << expr6 << " cursor=" << cursor6 << std::endl;
    assert(expr6 == "phi" && cursor6 == 3);

    handleTabCompletion(expr6, cursor6);
    std::cout << "Test 6.3 [no auto_brackets phi -> print(]: " << expr6 << " cursor=" << cursor6 << std::endl;
    assert(expr6 == "print(" && cursor6 == 6);

    // Test Case 6b: de -> deg2rad() / def , deg2 -> deg2rad(), d2 -> d2r()
    reset_tab();
    engineState().auto_brackets = true;
    std::string expr_deg2 = "deg2";
    int cursor_deg2 = 4;
    handleTabCompletion(expr_deg2, cursor_deg2);
    assert(expr_deg2 == "deg2rad()" && cursor_deg2 == 8);

    reset_tab();
    std::string expr_de = "de";
    int cursor_de = 2;
    handleTabCompletion(expr_de, cursor_de);
    assert(expr_de == "deg2rad()" && cursor_de == 8);
    handleTabCompletion(expr_de, cursor_de);
    assert(expr_de == "def " && cursor_de == 4);
    handleTabCompletion(expr_de, cursor_de);
    assert(expr_de == "deg2rad()" && cursor_de == 8);

    reset_tab();
    std::string expr_d2 = "d2";
    int cursor_d2 = 2;
    handleTabCompletion(expr_d2, cursor_d2);
    assert(expr_d2 == "d2r()" && cursor_d2 == 4);
    // Test Case 7: plot.xlim and plot.ylim vector support [min, max] and [min max]
    double plot_res = 0.0;
    std::string plot_err = "";
    bool handled = handlePlotCommands("plot.xlim([-5, 5])", plot_err, plot_res);
    assert(handled && plot_err.empty() && engineState().plot_manual_limits && engineState().plot_xlim_min == -5.0 && engineState().plot_xlim_max == 5.0);
    assert(engineState().plot_manual_x && !engineState().plot_manual_y);

    handled = handlePlotCommands("plot.ylim(5, 5)", plot_err, plot_res);
    assert(handled && !plot_err.empty() && !engineState().plot_manual_y);
    plot_err.clear();
    handled = handlePlotCommands("plot.xlim(5, -5)", plot_err, plot_res);
    assert(handled && !plot_err.empty() && engineState().plot_xlim_min == -5.0 && engineState().plot_xlim_max == 5.0);
    plot_err.clear();

    handled = handlePlotCommands("plot.ylim([-10 10])", plot_err, plot_res);
    assert(handled && plot_err.empty() && engineState().plot_manual_limits && engineState().plot_ylim_min == -10.0 && engineState().plot_ylim_max == 10.0);
    assert(engineState().plot_manual_x && engineState().plot_manual_y);

    handled = handlePlotCommands("plot.xlim(-2, 2)", plot_err, plot_res);
    assert(handled && plot_err.empty() && engineState().plot_manual_limits && engineState().plot_xlim_min == -2.0 && engineState().plot_xlim_max == 2.0);

    // Test Case 8: conv(...) units autocomplete (mm, mmHg, mm^2, mm2, std_mm2)
    reset_tab();
    engineState().auto_brackets = true;
    std::string expr8 = "conv(10, mm";
    int cursor8 = 11;
    handleTabCompletion(expr8, cursor8);
    std::cout << "Test 8.1 [conv(10, mm -> mm]: " << expr8 << " cursor=" << cursor8 << std::endl;
    assert(expr8 == "conv(10, mm" && cursor8 == 11);

    handleTabCompletion(expr8, cursor8);
    std::cout << "Test 8.2 [conv(10, mm -> mmHg]: " << expr8 << " cursor=" << cursor8 << std::endl;
    assert(expr8 == "conv(10, mmHg" && cursor8 == 13);

    handleTabCompletion(expr8, cursor8);
    std::cout << "Test 8.3 [conv(10, mmHg -> mm^2]: " << expr8 << " cursor=" << cursor8 << std::endl;
    assert(expr8 == "conv(10, mm^2" && cursor8 == 13);

    // Test Case 8.4: Dimension-aware filter - length source "mm" completing "k" gives "km", NOT "kg"
    reset_tab();
    std::string expr8_len = "conv(10, \"mm\", k";
    int cursor8_len = 16;
    handleTabCompletion(expr8_len, cursor8_len);
    std::cout << "Test 8.4 [conv(10, \"mm\", k -> km]: " << expr8_len << " cursor=" << cursor8_len << std::endl;
    assert(expr8_len == "conv(10, \"mm\", km" && cursor8_len == 17);

    // Test Case 8.5: Dimension-aware filter - pressure source "mmHg" completing "a" gives "atm"
    reset_tab();
    std::string expr8_press = "conv(10, \"mmHg\", a";
    int cursor8_press = 18;
    handleTabCompletion(expr8_press, cursor8_press);
    std::cout << "Test 8.5 [conv(10, \"mmHg\", a -> atm]: " << expr8_press << " cursor=" << cursor8_press << std::endl;
    assert(expr8_press == "conv(10, \"mmHg\", atm" && cursor8_press == 20);

    // Test Case 8.6: Dimension-aware filter - mass source "kg" completing "p" gives "pound", NOT "Pa" or "psi"
    reset_tab();
    std::string expr8_mass = "conv(10, \"kg\", p";
    int cursor8_mass = 16;
    handleTabCompletion(expr8_mass, cursor8_mass);
    std::cout << "Test 8.6 [conv(10, \"kg\", p -> pound]: " << expr8_mass << " cursor=" << cursor8_mass << std::endl;
    assert(expr8_mass == "conv(10, \"kg\", pound" && cursor8_mass == 20);

    // Test Case 8.7: Dimension-aware filter - wire/area source "awg" completing "m" gives "mm^2"
    reset_tab();
    std::string expr8_wire = "conv(10, \"awg\", m";
    int cursor8_wire = 17;
    handleTabCompletion(expr8_wire, cursor8_wire);
    std::cout << "Test 8.7 [conv(10, \"awg\", m -> mm^2]: " << expr8_wire << " cursor=" << cursor8_wire << std::endl;
    assert(expr8_wire == "conv(10, \"awg\", mm^2" && cursor8_wire == 20);

    // Test Case 9: mmHg unit conversion verification
    ConvResult c_mmhg = handleConv(760, "mmHg", "atm", units_dat);
    std::cout << "conv(760, \"mmHg\", \"atm\") = " << c_mmhg.value << " " << c_mmhg.unitStr << std::endl;
    assert(c_mmhg.success && std::abs(c_mmhg.value - 1.0) < 0.001);

    ConvResult c_mmhg_low = handleConv(760, "mmhg", "atm", units_dat);
    assert(c_mmhg_low.success && std::abs(c_mmhg_low.value - 1.0) < 0.001);

    // Test Case 10: Trigonometric & Hyperbolic functions test suite
    engineState().degreesMode = true;
    double t_sin = evaluate("sin(30)", err);
    assert(err.empty() && std::abs(t_sin - 0.5) < 1e-6);

    double t_cos = evaluate("cos(60)", err);
    assert(err.empty() && std::abs(t_cos - 0.5) < 1e-6);

    double t_tan = evaluate("tan(45)", err);
    assert(err.empty() && std::abs(t_tan - 1.0) < 1e-6);

    double t_ctan = evaluate("ctan(45)", err);
    assert(err.empty() && std::abs(t_ctan - 1.0) < 1e-6);

    double t_cot = evaluate("cot(45)", err);
    assert(err.empty() && std::abs(t_cot - 1.0) < 1e-6);

    double t_asin = evaluate("asin(0.5)", err);
    assert(err.empty() && std::abs(t_asin - 30.0) < 1e-6);

    double t_acos = evaluate("acos(0.5)", err);
    assert(err.empty() && std::abs(t_acos - 60.0) < 1e-6);

    double t_atan = evaluate("atan(1.0)", err);
    assert(err.empty() && std::abs(t_atan - 45.0) < 1e-6);

    double t_atan2 = evaluate("atan2(1.0, 1.0)", err);
    assert(err.empty() && std::abs(t_atan2 - 45.0) < 1e-6);

    // Domain errors for asin/acos (|x| <= 1)
    evaluate("asin(2)", err);
    assert(err == "Domain Error: asin(x) requires -1 <= x <= 1");

    evaluate("asin(-2)", err);
    assert(err == "Domain Error: asin(x) requires -1 <= x <= 1");

    evaluate("acos(1.5)", err);
    assert(err == "Domain Error: acos(x) requires -1 <= x <= 1");

    evaluate("acos(-1.5)", err);
    assert(err == "Domain Error: acos(x) requires -1 <= x <= 1");

    // Trig in Radians Mode
    engineState().degreesMode = false;
    double t_sin_rad = evaluate("sin(pi / 6)", err);
    assert(err.empty() && std::abs(t_sin_rad - 0.5) < 1e-6);

    double t_asin_rad = evaluate("asin(1)", err);
    assert(err.empty() && std::abs(t_asin_rad - M_PI / 2.0) < 1e-6);

    double t_atan2_rad = evaluate("atan2(1, 1)", err);
    assert(err.empty() && std::abs(t_atan2_rad - M_PI / 4.0) < 1e-6);
    engineState().degreesMode = true;

    // Hyperbolic & Inverse Hyperbolic
    double t_sinh0 = evaluate("sinh(0)", err);
    assert(err.empty() && std::abs(t_sinh0 - 0.0) < 1e-6);

    double t_cosh0 = evaluate("cosh(0)", err);
    assert(err.empty() && std::abs(t_cosh0 - 1.0) < 1e-6);

    double t_tanh0 = evaluate("tanh(0)", err);
    assert(err.empty() && std::abs(t_tanh0 - 0.0) < 1e-6);

    double t_asinh = evaluate("asinh(sinh(1.5))", err);
    assert(err.empty() && std::abs(t_asinh - 1.5) < 1e-4);

    double t_ahsin = evaluate("ahsin(sinh(1.5))", err);
    assert(err.empty() && std::abs(t_ahsin - 1.5) < 1e-4);

    double t_ahcos = evaluate("ahcos(cosh(2.0))", err);
    assert(err.empty() && std::abs(t_ahcos - 2.0) < 1e-4);

    double t_ahtan = evaluate("ahtan(tanh(0.75))", err);
    assert(err.empty() && std::abs(t_ahtan - 0.75) < 1e-4);

    double t_arcsin = evaluate("arcsin(0.5)", err);
    assert(err.empty() && std::abs(t_arcsin - 30.0) < 1e-4);

    double t_tg = evaluate("tg(45)", err);
    assert(err.empty() && std::abs(t_tg - 1.0) < 1e-4);

    double t_ctg = evaluate("ctg(45)", err);
    assert(err.empty() && std::abs(t_ctg - 1.0) < 1e-4);

    double t_sec = evaluate("sec(60)", err);
    assert(err.empty() && std::abs(t_sec - 2.0) < 1e-4);

    double t_csc = evaluate("csc(30)", err);
    assert(err.empty() && std::abs(t_csc - 2.0) < 1e-4);

    double t_cosec = evaluate("cosec(30)", err);
    assert(err.empty() && std::abs(t_cosec - 2.0) < 1e-4);

    double t_arctg = evaluate("arctg(1)", err);
    assert(err.empty() && std::abs(t_arctg - 45.0) < 1e-4);

    double t_arcctg = evaluate("arcctg(1)", err);
    assert(err.empty() && std::abs(t_arcctg - 45.0) < 1e-4);

    double t_arcsec = evaluate("arcsec(2)", err);
    assert(err.empty() && std::abs(t_arcsec - 60.0) < 1e-4);

    double t_arccsc = evaluate("arccsc(2)", err);
    assert(err.empty() && std::abs(t_arccsc - 30.0) < 1e-4);

    double t_arccosec = evaluate("arccosec(2)", err);
    assert(err.empty() && std::abs(t_arccosec - 30.0) < 1e-4);

    double t_sh = evaluate("sh(0)", err);
    assert(err.empty() && std::abs(t_sh - 0.0) < 1e-6);

    double t_ch = evaluate("ch(0)", err);
    assert(err.empty() && std::abs(t_ch - 1.0) < 1e-6);

    double t_th = evaluate("th(0)", err);
    assert(err.empty() && std::abs(t_th - 0.0) < 1e-6);

    double t_sech = evaluate("sech(0)", err);
    assert(err.empty() && std::abs(t_sech - 1.0) < 1e-6);

    double t_coth = evaluate("coth(1.5)", err);
    assert(err.empty() && std::abs(t_coth - (1.0 / std::tanh(1.5))) < 1e-4);

    double t_csch = evaluate("csch(1.5)", err);
    assert(err.empty() && std::abs(t_csch - (1.0 / std::sinh(1.5))) < 1e-4);

    double t_acoth = evaluate("acoth(coth(1.5))", err);
    assert(err.empty() && std::abs(t_acoth - 1.5) < 1e-4);

    double t_asech = evaluate("asech(sech(0.5))", err);
    assert(err.empty() && std::abs(t_asech - 0.5) < 1e-4);

    double t_acsch = evaluate("acsch(csch(1.5))", err);
    assert(err.empty() && std::abs(t_acsch - 1.5) < 1e-4);

    double t_arccosech = evaluate("arccosech(csch(1.5))", err);
    assert(err.empty() && std::abs(t_arccosech - 1.5) < 1e-4);

    double t_acosh = evaluate("acosh(cosh(2.0))", err);
    assert(err.empty() && std::abs(t_acosh - 2.0) < 1e-4);

    double t_atanh = evaluate("atanh(tanh(0.75))", err);
    assert(err.empty() && std::abs(t_atanh - 0.75) < 1e-4);

    // Hyperbolic domain errors
    evaluate("acosh(0.5)", err); // acosh defined for x >= 1
    assert(err == "Domain Error: acosh(x) requires x >= 1");

    evaluate("atanh(1.5)", err); // atanh defined for |x| < 1
    assert(err == "Domain Error: atanh(x) requires -1 < x < 1");

    evaluate("atanh(-2.0)", err);
    assert(err == "Domain Error: atanh(x) requires -1 < x < 1");

    // Additional domain error checks for sqrt, log, ln, fact
    evaluate("sqrt(-4)", err);
    assert(err == "Domain Error: sqrt(x) requires x >= 0");

    evaluate("log(-10)", err);
    assert(err == "Domain Error: log(x) requires x > 0");

    evaluate("ln(0)", err);
    assert(err == "Domain Error: ln(x) requires x > 0");

    evaluate("fact(-2)", err);
    assert(err == "Domain Error: fact(n) requires n >= 0");

    // Domain errors for new trigonometric and hyperbolic functions
    evaluate("asec(0.5)", err);
    assert(err == "Domain Error: asec(x) requires |x| >= 1");

    evaluate("arcsec(-0.5)", err);
    assert(err == "Domain Error: asec(x) requires |x| >= 1");

    evaluate("acsc(0.5)", err);
    assert(err == "Domain Error: acsc(x) requires |x| >= 1");

    evaluate("arccsc(-0.5)", err);
    assert(err == "Domain Error: acsc(x) requires |x| >= 1");

    evaluate("coth(0)", err);
    assert(err == "Domain Error: coth(x) requires x != 0");

    evaluate("csch(0)", err);
    assert(err == "Domain Error: csch(x) requires x != 0");

    evaluate("acoth(0.5)", err);
    assert(err == "Domain Error: acoth(x) requires |x| > 1");

    evaluate("acoth(1.0)", err);
    assert(err == "Domain Error: acoth(x) requires |x| > 1");

    evaluate("asech(0)", err);
    assert(err == "Domain Error: asech(x) requires 0 < x <= 1");

    evaluate("asech(2)", err);
    assert(err == "Domain Error: asech(x) requires 0 < x <= 1");

    evaluate("acsch(0)", err);
    assert(err == "Domain Error: acsch(x) requires x != 0");

    // Singularity domain errors (degrees mode)
    engineState().degreesMode = true;
    evaluate("tan(90)", err);
    assert(err == "Domain Error: tan(x) undefined when cos(x) = 0");

    evaluate("tg(90)", err);
    assert(err == "Domain Error: tan(x) undefined when cos(x) = 0");

    evaluate("sec(90)", err);
    assert(err == "Domain Error: sec(x) undefined when cos(x) = 0");

    evaluate("csc(0)", err);
    assert(err == "Domain Error: csc(x) undefined when sin(x) = 0");

    evaluate("cot(0)", err);
    assert(err == "Domain Error: cot(x) undefined when sin(x) = 0");

    evaluate("ctg(0)", err);
    assert(err == "Domain Error: cot(x) undefined when sin(x) = 0");

    // Test Case 11: Acceleration conversions
    ConvResult c_acc1 = handleConv(1, "m/s^2", "cm/s^2", units_dat);
    assert(c_acc1.success && std::abs(c_acc1.value - 100.0) < 1e-3);

    ConvResult c_acc2 = handleConv(1, "g0", "m/s^2", units_dat);
    assert(c_acc2.success && std::abs(c_acc2.value - 9.80665) < 1e-3);

    ConvResult c_acc3 = handleConv(9.80665, "m/s^2", "g", units_dat);
    assert(c_acc3.success && std::abs(c_acc3.value - 1.0) < 1e-3);
    auto grams = handleConv(1, "g", "kg", units_dat);
    assert(grams.success && std::abs(grams.value - 0.001) < 1e-9);
    auto fps2 = handleConv(1, "fps2", "m/s^2", units_dat);
    assert(fps2.success && std::abs(fps2.value - 0.3048) < 1e-9);
    assert(handleConv(1, "mps", "km/hr", units_dat).success);
    assert(handleConv(1, "psf", "Pa", units_dat).success);
    assert(handleConv(1, "gpa", "Pa", units_dat).success);
    auto knots = handleConv(1, "kt", "m/s", units_dat);
    assert(knots.success && std::abs(knots.value - 1852.0 / 3600.0) < 1e-9);
    auto milesPerHour = handleConv(1, "mi/h", "m/s", units_dat);
    assert(milesPerHour.success && std::abs(milesPerHour.value - 0.44704) < 1e-9);

    ConvResult c_acc4 = handleConv(100, "Gal", "m/s^2", units_dat);
    assert(c_acc4.success && std::abs(c_acc4.value - 1.0) < 1e-3);

    for (const auto& tc : std::vector<std::tuple<double, std::string, std::string, double>>{
             {1, "Mm", "m", 1e6}, {1, "Mg", "kg", 1000}, {1, "Ms", "s", 1e6},
             {1, "mPa", "Pa", .001}, {5, "MW", "dBm", 96.989700043},
             {1, "MV", "dBmV", 180}, {0, "dBm", "W", .001},
             {10, "dBW", "mW", 10000}, {1, "gal", "in^3", 231}, {1, "ton", "lb", 2000}}) {
        auto result = handleConv(std::get<0>(tc), std::get<1>(tc), std::get<2>(tc), units_dat);
        if (!result.success || std::abs(result.value - std::get<3>(tc)) > std::max(1e-6, std::abs(std::get<3>(tc)) * 1e-6)) {
            std::cerr << "Conversion failed: " << std::get<1>(tc) << " -> " << std::get<2>(tc) << ": " << result.value << " " << result.errorMsg << std::endl;
            assert(false);
        }
    }
    for (const auto& tc : std::vector<std::tuple<double, std::string, std::string>>{
             {9.96, "smd3", "100"}, {.04, "smd3", "R04"}, {47, "smd4", "47R0"}, {99.999, "smd4", "1000"}}) {
        auto result = handleConv(std::get<0>(tc), std::get<1>(tc), "", units_dat);
        if (!result.success || result.unitStr != std::get<2>(tc)) std::cerr << "Code failed: " << std::get<0>(tc) << " -> " << std::get<1>(tc) << ": " << result.unitStr << " " << result.errorMsg << std::endl;
        assert(result.success && result.unitStr == std::get<2>(tc));
    }
    assert(!handleConv(1e9, "eia96", "", units_dat).success);
    assert(!handleConv(9.99e10, "smd3", "", units_dat).success);
    assert(!handleConv(9.999e11, "smd4", "", units_dat).success);

    ConvResult c_acc5 = handleConv(1, "gravity", "ft/s^2", units_dat);
    assert(c_acc5.success && std::abs(c_acc5.value - 32.174) < 0.05);

    // Test Case 12: Acceleration dimension-aware tab completion
    reset_tab();
    std::string expr_acc = "conv(10, \"m/s^2\", c";
    int cursor_acc = 19;
    handleTabCompletion(expr_acc, cursor_acc);
    assert(expr_acc == "conv(10, \"m/s^2\", cm/s^2" && cursor_acc == 24);

    // Test Case 13: Angle operations and output formatting
    engineState().degreesMode = true;
    double val_asin = evaluateInput("asin(0.5)", err, isDef);
    assert(err.empty() && std::abs(val_asin - 30.0) < 1e-6);
    assert(engineState().g_last_result_angle == AngleUnit::DEG);

    engineState().degreesMode = false;
    double val_asin_rad = evaluateInput("asin(0.5)", err, isDef);
    assert(err.empty() && std::abs(val_asin_rad - 0.5235987) < 1e-5);
    assert(engineState().g_last_result_angle == AngleUnit::RAD);

    // Variable assignment x = asin(0.5)
    engineState().degreesMode = true;
    evaluateInput("x_angle = asin(0.5)", err, isDef);
    assert(err.empty() && isDef);
    assert(engineState().g_last_result_angle == AngleUnit::DEG);

    // Conversions deg2rad and rad2deg
    evaluateInput("r_angle = deg2rad(180)", err, isDef);
    assert(err.empty() && isDef);
    assert(engineState().g_last_result_angle == AngleUnit::RAD);

    evaluateInput("d_angle = rad2deg(pi)", err, isDef);
    assert(err.empty() && isDef);
    assert(engineState().g_last_result_angle == AngleUnit::DEG);

    // Non-angle trig operations must reset g_last_result_angle to NONE
    evaluateInput("sin(30)", err, isDef);
    assert(engineState().g_last_result_angle == AngleUnit::NONE);

    // Test Case 14: Tab completion caching verification
    reset_tab();
    std::string test_expr = "p";
    int test_cursor = 1;
    handleTabCompletion(test_expr, test_cursor);
    assert(test_expr == "pi");
    handleTabCompletion(test_expr, test_cursor);
    assert(test_expr == "phi");
    handleTabCompletion(test_expr, test_cursor);
    assert(test_expr == "print()");

    // Test Case 15: logb(x, base) argument order and domain validation
    double v_logb1 = evaluate("logb(8, 2)", err);
    assert(err.empty() && std::abs(v_logb1 - 3.0) < 1e-6);
    double v_logb2 = evaluate("logb(100, 10)", err);
    assert(err.empty() && std::abs(v_logb2 - 2.0) < 1e-6);
    evaluate("logb(8, 1)", err);
    assert(err.find("Domain Error") != std::string::npos);
    evaluate("logb(-8, 2)", err);
    assert(err.find("Domain Error") != std::string::npos);

    // Test Case 16: Combinatorics, Factorial, Fibonacci bounds & multiplicative C/P
    evaluate("fact(3.5)", err);
    assert(err == "Domain Error: fact(n) requires integer n >= 0");
    evaluate("fact(171)", err);
    assert(err == "Overflow Error: fact(n) exceeds maximum limit (n <= 170)");
    evaluate("fact(1e9)", err);
    assert(err == "Overflow Error: fact(n) exceeds maximum limit (n <= 170)");
    evaluate("fib(1477)", err);
    assert(err == "Overflow Error: fib(n) exceeds maximum limit (n <= 1476)");
    evaluate("fib(2.5)", err);
    assert(err == "Domain Error: fib(n) requires integer n >= 0");

    // C(180, 2) must not overflow intermediate factorial (180 * 179 / 2 = 16110)
    double v_c180 = evaluate("C(180, 2)", err);
    assert(err.empty() && std::abs(v_c180 - 16110.0) < 1e-6);
    double v_cnk = evaluate("Cnk(10, 3)", err);
    assert(err.empty() && std::abs(v_cnk - 120.0) < 1e-6);
    double v_p180 = evaluate("P(180, 2)", err);
    assert(err.empty() && std::abs(v_p180 - 32220.0) < 1e-6);

    // Variadic gcd / lcm / sum / prod / min / max
    double v_gcd3 = evaluate("gcd(12, 18, 30)", err);
    assert(err.empty() && std::abs(v_gcd3 - 6.0) < 1e-6);
    double v_lcm3 = evaluate("lcm(4, 6, 8)", err);
    assert(err.empty() && std::abs(v_lcm3 - 24.0) < 1e-6);
    double v_sum_var = evaluate("sum(1, 2, 3, 4, 5)", err);
    assert(err.empty() && std::abs(v_sum_var - 15.0) < 1e-6);
    double v_prod_var = evaluate("prod(2, 3, 4)", err);
    assert(err.empty() && std::abs(v_prod_var - 24.0) < 1e-6);

    // Overflow vs Division by Zero distinction
    evaluate("1 / 0", err);
    assert(err == "Division by Zero");
    evaluate("exp(1000)", err);
    assert(err == "Overflow Error");

    // Test Case 17: Inline conditionals (if, iff) and inline loops (sum, prod, min, max)
    double v_if1 = evaluateInput("if(5 > 3, 10, 20)", err, isDef);
    assert(err.empty() && std::abs(v_if1 - 10.0) < 1e-6);
    double v_if2 = evaluateInput("if(2 > 5, 10, 20)", err, isDef);
    assert(err.empty() && std::abs(v_if2 - 20.0) < 1e-6);
    double v_iff = evaluateInput("iff(2 > 5, 10, 3 == 3, 42, 99)", err, isDef);
    assert(err.empty() && std::abs(v_iff - 42.0) < 1e-6);

    double v_loop_sum = evaluateInput("sum(i, 1, 10, i^2)", err, isDef);
    assert(err.empty() && std::abs(v_loop_sum - 385.0) < 1e-6);
    double v_loop_prod = evaluateInput("prod(i, 1, 5, i)", err, isDef);
    assert(err.empty() && std::abs(v_loop_prod - 120.0) < 1e-6);
    double v_loop_min = evaluateInput("min(i, 1, 5, (i - 3)^2 + 2)", err, isDef);
    assert(err.empty() && std::abs(v_loop_min - 2.0) < 1e-6);
    double v_loop_max = evaluateInput("max(i, 1, 5, i * 3)", err, isDef);
    assert(err.empty() && std::abs(v_loop_max - 15.0) < 1e-6);

    // Test Case 18: Vector & Array operations, inline indexing, domain errors in arrays
    evaluateInput("arr = [10, 20, 30, 40]", err, isDef);
    assert(err.empty() && isDef && engineState().user_arrays.count("arr") && engineState().user_arrays["arr"].size() == 4);
    double v_arr_idx = evaluateInput("arr[2]", err, isDef);
    assert(err.empty() && std::abs(v_arr_idx - 20.0) < 1e-6);
    double v_arr_sum = evaluateInput("sum(arr)", err, isDef);
    assert(err.empty() && std::abs(v_arr_sum - 100.0) < 1e-6);
    double v_arr_prod = evaluateInput("prod(arr)", err, isDef);
    assert(err.empty() && std::abs(v_arr_prod - 240000.0) < 1e-6);
    double v_arr_mean = evaluateInput("mean(arr)", err, isDef);
    assert(err.empty() && std::abs(v_arr_mean - 25.0) < 1e-6);

    // Inline range indexing (1:2:10)[2] -> 3 (1-based)
    double v_inline_idx = evaluateInput("(1:2:10)[2]", err, isDef);
    assert(err.empty() && std::abs(v_inline_idx - 3.0) < 1e-6);

    // Dot product of two equal-size vectors
    evaluateInput("vecA = [1, 2, 3]", err, isDef);
    assert(err.empty());
    evaluateInput("vecB = [4, 5, 6]", err, isDef);
    assert(err.empty());
    double v_dot = evaluateInput("vecA * vecB", err, isDef);
    assert(err.empty() && std::abs(v_dot - 32.0) < 1e-6);

    // Vectorized function evaluation & domain error propagation
    engineState().degreesMode = true;
    evaluateInput("sin([0, 30, 90])", err, isDef);
    assert(err.empty() && engineState().g_last_eval_is_array && engineState().g_last_eval_array.size() == 3);
    assert(std::abs(engineState().g_last_eval_array[1] - 0.5) < 1e-6 && std::abs(engineState().g_last_eval_array[2] - 1.0) < 1e-6);

    evaluateInput("asec([0.5, 2])", err, isDef);
    assert(err == "Domain Error: asec(x) requires |x| >= 1");

    // Test Case 19: Units adapter - times_amp/times_v -> dB (20*log10) vs times -> dB (10*log10) & tempC/tempF/tempK
    ConvResult c_amp_db = handleConv(10, "times_amp", "dB", units_dat);
    assert(c_amp_db.success && std::abs(c_amp_db.value - 20.0) < 1e-6);
    ConvResult c_v_db = handleConv(10, "times_v", "dB", units_dat);
    assert(c_v_db.success && std::abs(c_v_db.value - 20.0) < 1e-6);
    ConvResult c_pow_db = handleConv(10, "times", "dB", units_dat);
    assert(c_pow_db.success && std::abs(c_pow_db.value - 10.0) < 1e-6);
    ConvResult c_neg_db = handleConv(-5, "times", "dB", units_dat);
    assert(!c_neg_db.success && c_neg_db.errorMsg.find("Domain Error") != std::string::npos);

    ConvResult c_temp1 = handleConv(100, "tempC", "tempF", units_dat);
    assert(c_temp1.success && std::abs(c_temp1.value - 212.0) < 1e-4);
    ConvResult c_temp2 = handleConv(0, "tempC", "tempK", units_dat);
    assert(c_temp2.success && std::abs(c_temp2.value - 273.15) < 1e-4);
    ConvResult c_temp3 = handleConv(0, "tempK", "degR", units_dat);
    assert(c_temp3.success && std::abs(c_temp3.value - 0.0) < 1e-4);

    // Test Case 20: Script engine - session variable preservation, loops, conditionals, recursion
    engineState().user_args.clear();
    engineState().user_arrays.clear();
    evaluateInput("session_var = 100", err, isDef);
    assert(err.empty());

    std::string script_code =
        "fn fact_rec(n) {\n"
        "  if (n <= 1) {\n"
        "    return 1\n"
        "  }\n"
        "  return n * fact_rec(n - 1)\n"
        "}\n"
        "total = session_var\n"
        "for (i = 1; i <= 5; i = i + 1) {\n"
        "  total = total + i\n"
        "}\n"
        "r = fact_rec(5)\n"
        "print(total + r)\n";
    runScript(script_code);
    std::string script_out;
    for (const auto& line : engineState().script_console_output) script_out += line + "\n";
    std::cout << "Script output: " << script_out;
    // session_var(100) + sum(1..5)(15) + fact_rec(5)(120) = 235
    assert(script_out.find("235") != std::string::npos);
    // Verify session_var was NOT wiped by runScript
    double v_sess = evaluateInput("session_var", err, isDef);
    assert(err.empty() && std::abs(v_sess - 100.0) < 1e-6);

    // Test Case 21: getConvUnitMatches dimension filtering
    auto len_units = getConvUnitMatches("mm", "k");
    assert(!len_units.empty() && len_units[0] == "km");

    // Only the latest 128 history values are registered with the expression parser.
    engineState().history.clear();
    for (int i = 1; i <= 130; ++i) engineState().history.push_back(static_cast<double>(i));
    err.clear();
    assert(evaluate("e3", err) == 3.0 && err.empty());
    err.clear();
    assert(evaluate("e130", err) == 130.0 && err.empty());
    err.clear();
    evaluate("e2", err);
    assert(!err.empty());

    const auto split_args = expression::splitArguments("max(1,2), [3,4], \"x,y\"");
    assert(split_args.size() == 3 && split_args[0] == "max(1,2)" &&
           split_args[1] == "[3,4]" && split_args[2] == "\"x,y\"");
    const auto escaped_args = expression::splitArguments("\"a\\\",b\",c");
    assert(escaped_args.size() == 2 && escaped_args[1] == "c");
    clearHistory();
    for (int i = 1; i <= 130; ++i) assert(appendHistory(i) == static_cast<size_t>(i));
    assert(engineState().history.size() == 128 && engineState().history_first_number == 3);
    err.clear(); assert(evaluate("e3", err) == 3.0 && err.empty());
    err.clear(); assert(evaluate("e130", err) == 130.0 && err.empty());
    err.clear(); evaluate("e2", err); assert(!err.empty());
    err.clear(); evaluateInput("e3=99", err, isDef); assert(!err.empty());
    runScript("e3++");
    assert(!engineState().script_console_output.empty() && engineState().script_console_output[0].find("read-only") != std::string::npos);
    runScript("print(\"e3=1\")");
    assert(!engineState().script_console_output.empty() && engineState().script_console_output[0] == "e3=1");
    clearHistory();
    assert(appendHistory(42) == 1);
    double ratio = 0;
    char si_unit[64] = {}, conversion_error[128] = {};
    assert(bridge_convert(1, "cm", "m", &ratio, si_unit, sizeof(si_unit), conversion_error, sizeof(conversion_error)) == 0);
    assert(std::abs(ratio - 0.01) < 1e-12);
    for (int i = 0; i < 100; ++i)
        assert(bridge_convert(1, "meter", "second", &ratio, si_unit, sizeof(si_unit), conversion_error, sizeof(conversion_error)) != 0);

    std::atomic<bool> conversions_ok{true};
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 4; ++worker) workers.emplace_back([&] {
        for (int i = 0; i < 100; ++i) {
            double converted = 0;
            char unit[64] = {}, error[128] = {};
            if (bridge_convert(1, "cm", "m", &converted, unit, sizeof(unit), error, sizeof(error)) != 0 ||
                std::abs(converted - 0.01) > 1e-12) conversions_ok = false;
        }
    });
    for (auto& worker : workers) worker.join();
    assert(conversions_ok);
    assert(bridge_test_fatal() == 1);
    assert(bridge_convert(1, "cm", "m", &ratio, si_unit, sizeof(si_unit), conversion_error, sizeof(conversion_error)) != 0);

    std::cout << "[Droidulator Native Test] ALL TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
