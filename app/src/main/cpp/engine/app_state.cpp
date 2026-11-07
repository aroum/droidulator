#include "app_state.h"
#include <algorithm>
#include <cctype>

// Unified shared state definitions for Droidulator engine
std::vector<double> history;
size_t history_first_number = 1;
size_t appendHistory(double value) {
    if (history.size() >= MAX_HISTORY_VALUES) {
        const size_t excess = history.size() - MAX_HISTORY_VALUES + 1;
        history.erase(history.begin(), history.begin() + excess);
        history_first_number += excess;
    }
    history.push_back(value);
    return history_first_number + history.size() - 1;
}
void clearHistory() { history.clear(); history_first_number = 1; }
bool isHistoryName(const std::string& name) {
    return name.size() > 1 && name[0] == 'e' &&
        std::all_of(name.begin() + 1, name.end(), [](unsigned char c) { return std::isdigit(c); });
}
std::vector<UserArg> user_args;
std::vector<UserConst> user_consts;
std::vector<std::string> user_funcs;
std::vector<CustomScriptFunc> user_script_funcs;
std::map<std::string, std::vector<double>> user_arrays;
std::map<std::string, std::string> user_named_scripts;
std::vector<std::string> script_console_output;
bool use_thousands_sep = false;
bool degreesMode = true;
bool auto_brackets = true;

std::vector<std::string> autocomplete_words = {
    "sin(", "cos(", "tan(", "ctan(", "cot(", "asin(", "acos(", "atan(", "atan2(",
    "arcsin(", "arccos(", "arctan(",
    "sinh(", "cosh(", "tanh(", "asinh(", "acosh(", "atanh(",
    "ahsin(", "ahcos(", "ahtan(", "arcsinh(", "arccosh(", "arctanh(",
    "deg2rad(", "d2r(", "rad2deg(", "r2d(",
    "sqrt(", "cbrt(", "exp(", "ln(", "log(", "log10(", "log2(", "logb(",
    "abs(", "ceil(", "floor(", "round(", "trunc(", "sgn(", "mod(",
    "mean(", "median(", "mode(", "var(", "std(", "dot(", "len(", "rUni(", "rNor(",
    "fact(", "C(", "P(", "gcd(", "lcm(", "fib(",
    "print(", "run(", "conv(", "help(", "help()", "const ", "def ", "fn ",
    "pi", "e", "phi",
    "plot(", "plot.show()", "plot.close()", "plot.hold(", "plot.xlim(", "plot.ylim(",
    "tg(", "ctg(", "sec(", "csc(", "cosec(",
    "atg(", "arctg(", "acot(", "arccot(", "actg(", "arcctg(", "actan(", "arcctan(",
    "asec(", "arcsec(", "acsc(", "arccsc(", "acosec(", "arccosec(",
    "sh(", "ch(", "th(",
    "coth(", "cth(", "ctgh(", "ctanh(", "sech(", "sch(", "csch(", "cosech(",
    "arsh(", "arch(", "arth(", "artg(", "arctgh(",
    "acoth(", "ahcot(", "arccoth(", "arcoth(", "arcth(", "ahcoth(",
    "asech(", "ahsec(", "arcsech(", "arsech(", "ahsech(",
    "acsch(", "ahcsc(", "arccsch(", "arcsch(", "acosech(", "arccosech(", "ahcsch(",
    "Cnk(", "sum(", "prod(", "min(", "max(", "if(", "iff("
};
static const std::vector<std::string> builtin_autocomplete_words = autocomplete_words;
bool isReservedName(const std::string& name) {
    if (isHistoryName(name)) return true;
    // TinyExpr also provides built-ins that are not offered by autocomplete.
    static const std::vector<std::string> extraBuiltins = {
        "and", "or", "not", "xor", "average", "bitand", "bitor", "bitxor", "bitlshift", "bitrshift",
        "bitlrotate8", "bitrrotate8", "bitlrotate16", "bitrrotate16", "bitlrotate32", "bitrrotate32",
        "bitlrotate64", "bitrrotate64", "bitlrotate", "bitrrotate", "bitnot8", "bitnot16", "bitnot32",
        "bitnot64", "bitnot", "clamp", "combin", "cumipmt", "cumprinc", "db", "effect", "even",
        "fac", "false", "fv", "iserr", "iserror", "iseven", "isna", "isnan", "isodd", "ifs", "ipmt",
        "maxint", "na", "nan", "ncr", "nominal", "nper", "npr", "odd", "permut", "pmt", "pow",
        "power", "ppmt", "pv", "rand", "sign", "sqr", "supports32bit", "supports64bit", "tgamma", "true"
    };
    if (std::find(extraBuiltins.begin(), extraBuiltins.end(), name) != extraBuiltins.end()) return true;
    return std::find(builtin_autocomplete_words.begin(), builtin_autocomplete_words.end(), name) != builtin_autocomplete_words.end() ||
        std::find(builtin_autocomplete_words.begin(), builtin_autocomplete_words.end(), name + "(") != builtin_autocomplete_words.end();
}

void resetAutocompleteWords() { autocomplete_words = builtin_autocomplete_words; }
void removeUserAutocompleteWord(const std::string& word) {
    if (std::find(builtin_autocomplete_words.begin(), builtin_autocomplete_words.end(), word) == builtin_autocomplete_words.end())
        autocomplete_words.erase(std::remove(autocomplete_words.begin(), autocomplete_words.end(), word), autocomplete_words.end());
}
