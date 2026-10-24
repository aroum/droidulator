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
    "sin(", "cos(", "tan(", "ctan(", "cot(", "asin(", "acos(", "atan(", "atan2(",
    "sinh(", "cosh(", "tanh(", "asinh(", "acosh(", "atanh(",
    "deg2rad(", "d2r(", "rad2deg(", "r2d(",
    "sqrt(", "cbrt(", "exp(", "ln(", "log(", "log10(", "log2(", "logb(",
    "abs(", "ceil(", "floor(", "round(", "trunc(", "sgn(", "mod(",
    "mean(", "median(", "mode(", "var(", "std(", "dot(", "len(", "rUni(", "rNor(",
    "fact(", "C(", "P(", "gcd(", "lcm(", "fib(",
    "print(", "run(", "conv(", "help(", "help()", "const ", "def ", "fn ",
    "pi", "e", "phi",
    "plot(", "plot.show()", "plot.close()", "plot.hold(", "plot.xlim(", "plot.ylim("
};
