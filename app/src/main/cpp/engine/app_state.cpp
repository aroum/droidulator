#include "app_state.h"

// Unified shared state definitions for Droidulator engine
std::vector<double> history;
std::vector<UserArg> user_args;
std::vector<UserConst> user_consts;
std::vector<std::string> user_funcs;
std::vector<CustomScriptFunc> user_script_funcs;
std::map<std::string, std::vector<double>> user_arrays;
std::map<std::string, std::string> user_named_scripts;
std::vector<std::string> script_console_output;
bool use_thousands_sep = false;
bool sticky_mode = false;
bool degreesMode = true;
bool auto_brackets = true;

std::vector<std::string> autocomplete_words = {
    "sin(", "cos(", "tan(", "ctan(", "asin(", "acos(", "atan(",
    "sinh(", "cosh(", "tanh(", "asinh(", "acosh(", "atanh(",
    "sqrt(", "ln(", "log(", "log2(", "logb(", "exp(", "cbrt(",
    "abs(", "ceil(", "floor(", "round(", "trunc(", "sgn(", "mod(",
    "mean(", "median(", "var(", "std(", "rUni(", "rNor(",
    "fact(", "C(", "P(", "gcd(", "lcm(", "fib(", "len(", "print(", "run(", "conv(", "help(", "help()", "const ",
    "pi", "e",
    "plot(", "plot.show()", "plot.close()", "plot.hold(", "plot.xlim(", "plot.ylim("
};
