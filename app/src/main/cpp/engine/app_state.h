#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

struct UserArg {
    std::string name;
    double val;
};

struct UserConst {
    std::string name;
    double val;
};

struct UserFunc {
    std::string name;
    std::vector<std::string> params;
    std::string body;
};

struct CustomScriptFunc {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> statements;
};

struct PlotLine {
    std::vector<double> x;
    std::vector<double> y;
    uint16_t color;
    std::string linestyle;
};


enum class AngleUnit {
    NONE,
    DEG,
    RAD
};

constexpr size_t MAX_HISTORY_VALUES = 128;
size_t appendHistory(double value);
void clearHistory();
bool isHistoryName(const std::string& name);
bool isReservedName(const std::string& name);
void resetAutocompleteWords();
void removeUserAutocompleteWord(const std::string& word);

// Each context owns one calculator session. Bind it on the calling thread for
// the duration of an operation, including callbacks into the evaluator.
struct EngineContext {
    EngineContext();
    std::vector<PlotLine> plot_lines{};
    bool plot_hold = false;
    bool plot_manual_limits = false;
    bool plot_manual_x = false;
    bool plot_manual_y = false;
    double plot_xlim_min = 0.0;
    double plot_xlim_max = 0.0;
    double plot_ylim_min = 0.0;
    double plot_ylim_max = 0.0;
    std::string g_math_domain_err = "";
    AngleUnit g_last_result_angle = AngleUnit::NONE;
    bool g_last_eval_is_array = false;
    std::vector<double> g_last_eval_array{};
    std::vector<CustomScriptFunc> user_script_funcs{};
    std::vector<double> history{};
    size_t history_first_number = 1;
    std::vector<UserArg> user_args{};
    std::vector<UserConst> user_consts{};
    std::vector<std::string> user_funcs{};
    std::map<std::string, std::vector<double>> user_arrays{};
    std::map<std::string, std::string> user_named_scripts{};
    std::vector<std::string> autocomplete_words;
    std::vector<std::string> script_console_output{};
    bool degreesMode = true;
    bool use_thousands_sep = false;
    bool auto_brackets = true;
    double script_return_val = 0.0;
    bool script_has_returned = false;
    std::vector<double> script_return_vec{};
    bool script_return_is_vec = false;
    bool s_in_array_fallback = false;
    std::string g_tab_orig_prefix = "";
    size_t g_tab_comp_start = 0;
    int g_tab_match_idx = -1;
    std::string g_tab_last_inserted = "";
    std::vector<std::string> g_tab_cached_matches{};
    int fmt_mode = 0;
    int fmt_precision = 6;
};

EngineContext& engineState();

class EngineContextScope {
public:
    explicit EngineContextScope(EngineContext& context);
    ~EngineContextScope();
    EngineContextScope(const EngineContextScope&) = delete;
    EngineContextScope& operator=(const EngineContextScope&) = delete;

private:
    EngineContext* previous;
};
