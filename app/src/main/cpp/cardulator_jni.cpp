#include <jni.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <new>
#include <exception>
#include <mutex>
#include <android/log.h>

#define LOG_TAG "CardulatorJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


#include "app_state.h"
#include "help_engine.h"
#include "math_funcs.h"
#include "plot_engine.h"
#include "script_engine.h"
#include "math_engine.h"
#include "units_adapter.h"


static std::recursive_mutex g_engine_mutex;

extern "C" {
static void throwJavaException(JNIEnv *env, const char *type, const char *message);

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeInit(JNIEnv *env, jobject thiz) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    clearHistory();
    user_args.clear();
    user_funcs.clear();
    user_script_funcs.clear();
    user_consts.clear();
    user_arrays.clear();
    script_console_output.clear();
    resetAutocompleteWords();
    resetTabState();
    degreesMode = true;
    LOGI("Cardulator Native Engine Initialized.");
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return;
}

static std::string toCppString(JNIEnv *env, jstring value) {
    if (!value) return {};
    const jsize length = env->GetStringLength(value);
    if (length == 0) return {};
    std::vector<jchar> utf16(static_cast<size_t>(length));
    env->GetStringRegion(value, 0, length, utf16.data());
    if (env->ExceptionCheck()) return {};
    std::string result;
    for (jsize i = 0; i < length; ++i) {
        uint32_t code = utf16[i];
        if (code >= 0xd800 && code <= 0xdbff && i + 1 < length && utf16[i + 1] >= 0xdc00 && utf16[i + 1] <= 0xdfff)
            code = 0x10000 + ((code - 0xd800) << 10) + (utf16[++i] - 0xdc00);
        else if (code >= 0xd800 && code <= 0xdfff) code = 0xfffd;
        if (code < 0x80) result.push_back(static_cast<char>(code));
        else if (code < 0x800) {
            result.push_back(static_cast<char>(0xc0 | (code >> 6)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else if (code < 0x10000) {
            result.push_back(static_cast<char>(0xe0 | (code >> 12)));
            result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else {
            result.push_back(static_cast<char>(0xf0 | (code >> 18)));
            result.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        }
    }
    return result;
}

static jstring toJavaString(JNIEnv *env, const std::string& value) {
    std::vector<jchar> output;
    const auto *p = reinterpret_cast<const unsigned char *>(value.data());
    const auto *end = p + value.size();
    while (p < end) {
        uint32_t code = *p++;
        if (code >= 0x80) {
            unsigned n = code >= 0xf0 && code <= 0xf4 ? 3 : code >= 0xe0 && code <= 0xef ? 2 : code >= 0xc2 && code <= 0xdf ? 1 : 0;
            uint32_t acc = code & (n == 3 ? 0x07 : n == 2 ? 0x0f : 0x1f);
            unsigned read = 0;
            while (read < n && p + read < end && (p[read] & 0xc0) == 0x80) { acc = (acc << 6) | (p[read] & 0x3f); ++read; }
            if (n && read == n && acc >= (n == 1 ? 0x80u : n == 2 ? 0x800u : 0x10000u) && acc <= 0x10ffff && !(acc >= 0xd800 && acc <= 0xdfff)) {
                code = acc; p += n;
            } else code = 0xfffd;
        }
        if (code <= 0xffff) output.push_back(static_cast<jchar>(code));
        else { code -= 0x10000; output.push_back(static_cast<jchar>(0xd800 | (code >> 10))); output.push_back(static_cast<jchar>(0xdc00 | (code & 0x3ff))); }
    }
    return env->NewString(output.data(), static_cast<jsize>(output.size()));
}

static void throwJavaException(JNIEnv *env, const char *type, const char *message) {
    if (env->ExceptionCheck()) return;
    jclass cls = env->FindClass(type);
    if (cls) env->ThrowNew(cls, message);
}

static std::string g_units_dat_path;

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeInitUnits(JNIEnv *env, jobject thiz, jstring path_jstr) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    g_units_dat_path = toCppString(env, path_jstr);
    if (bridge_init(g_units_dat_path.empty() ? nullptr : g_units_dat_path.c_str()) != 0) {
        throwJavaException(env, "java/lang/IllegalStateException", "Unable to initialize the units database");
        return;
    }
    LOGI("GNU Units initialised: %s", g_units_dat_path.c_str());
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return;
}

static bool g_last_has_num = false;
static double g_last_num_val = 0.0;
static std::string g_last_prefix;
static std::string g_last_suffix;

// Preview evaluation — evaluates formula without adding to history (used for live formula dialog preview)
JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeEvaluatePreview(JNIEnv *env, jobject thiz, jstring input_jstr) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    std::string expression = toCppString(env, input_jstr);
    if (expression.empty()) return toJavaString(env, "");

    std::string err;
    double val = evaluateWithoutHistory(expression, err);
    if (!err.empty()) return toJavaString(env, ("ERROR: " + err));
    return toJavaString(env, fmtNum(val));
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return nullptr;
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeEvaluate(JNIEnv *env, jobject thiz, jstring input_jstr) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    g_last_has_num = false;
    g_last_num_val = 0.0;
    g_last_prefix.clear();
    g_last_suffix.clear();

    std::string expression = toCppString(env, input_jstr);
    if (expression.empty()) return toJavaString(env, "");

    std::string err;
    bool isDef = false;
    double result = 0.0;
    std::string resultStr;

    std::string trimmed_expr = expression;
    trimmed_expr.erase(0, trimmed_expr.find_first_not_of(" \t"));
    trimmed_expr.erase(trimmed_expr.find_last_not_of(" \t") + 1);

    // 1. Handle print(...)
    if (trimmed_expr.rfind("print(", 0) == 0 && trimmed_expr.back() == ')') {
        std::string print_body = trimmed_expr.substr(6, trimmed_expr.size() - 7);
        std::string print_res = formatPrintString(print_body, err);
        if (!err.empty()) {
            return toJavaString(env, ("ERROR: " + err));
        }
        return toJavaString(env, print_res);
    }

    // 1b. Handle conv(value, "unit_a", ["unit_b"]) or [val, unit] = conv(...)
    std::string conv_expr = trimmed_expr;
    std::string assign_var1 = "", assign_var2 = "";
    bool is_assign = false;

    size_t eq_pos = trimmed_expr.find('=');
    if (eq_pos != std::string::npos && 
        trimmed_expr.find("==") == std::string::npos &&
        trimmed_expr.find(">=") == std::string::npos &&
        trimmed_expr.find("<=") == std::string::npos &&
        trimmed_expr.find("!=") == std::string::npos) {
        
        std::string lhs = trimmed_expr.substr(0, eq_pos);
        std::string rhs = trimmed_expr.substr(eq_pos + 1);
        lhs.erase(0, lhs.find_first_not_of(" \t"));
        lhs.erase(lhs.find_last_not_of(" \t") + 1);
        rhs.erase(0, rhs.find_first_not_of(" \t"));
        rhs.erase(rhs.find_last_not_of(" \t") + 1);

        if (rhs.rfind("conv(", 0) == 0 && rhs.back() == ')') {
            is_assign = true;
            conv_expr = rhs;

            // Check if LHS has multiple variables: val, unit = conv(...)
            size_t comma = lhs.find(',');
            if (comma != std::string::npos) {
                assign_var1 = lhs.substr(0, comma);
                assign_var2 = lhs.substr(comma + 1);
            } else {
                assign_var1 = lhs;
            }
            assign_var1.erase(0, assign_var1.find_first_not_of(" \t"));
            assign_var1.erase(assign_var1.find_last_not_of(" \t") + 1);
            assign_var2.erase(0, assign_var2.find_first_not_of(" \t"));
            assign_var2.erase(assign_var2.find_last_not_of(" \t") + 1);
        }
    }

    if (conv_expr.rfind("conv(", 0) == 0 && conv_expr.back() == ')') {
        std::string body = conv_expr.substr(5, conv_expr.size() - 6);
        std::vector<std::string> conv_args = splitPlotArgs(body);
        if (conv_args.size() < 2 || conv_args.size() > 3) {
            return toJavaString(env, "ERROR: conv requires 2 or 3 arguments: conv(val, \"unit_a\", [\"unit_b\"])");
        }

        std::string eval_err;
        double val = evaluate(conv_args[0], eval_err);
        std::string unit_a = conv_args[1];
        std::string unit_b = (conv_args.size() == 3) ? conv_args[2] : "";

        if (!eval_err.empty() || std::isnan(val)) {
            const std::string raw = conv_args[0];
            const bool quoted = raw.size() >= 2 && (raw.front() == '\'' || raw.front() == '"') && raw.back() == raw.front();
            const bool code = !raw.empty() && std::isdigit(static_cast<unsigned char>(raw[0])) &&
                std::all_of(raw.begin(), raw.end(), [](unsigned char c) { return std::isdigit(c) || c == 'R' || c == 'r' || (c >= 'A' && c <= 'Z'); });
            if (conv_args.size() != 2 || (!quoted && !code))
                return toJavaString(env, "ERROR: " + (eval_err.empty() ? std::string("Invalid conversion value") : eval_err));
            val = 0.0;
            unit_a = raw;
            unit_b = conv_args[1];
        }

        ConvResult c_res = handleConv(val, unit_a, unit_b);
        if (!c_res.success) {
            return toJavaString(env, ("ERROR: " + c_res.errorMsg));
        }

        if (is_assign) {
            // History names are reserved for computed results.
            if (isReservedName(assign_var1) || isReservedName(assign_var2))
                return toJavaString(env, "ERROR: Reserved name");
            // Save value to first variable
            if (!assign_var1.empty()) {
                auto it = std::find_if(user_args.begin(), user_args.end(), [&](const UserArg& a) {
                    return a.name == assign_var1;
                });
                if (it != user_args.end()) {
                    it->val = c_res.value;
                } else {
                    user_args.push_back({assign_var1, c_res.value});
                }
                if (std::find(autocomplete_words.begin(), autocomplete_words.end(), assign_var1) == autocomplete_words.end()) {
                    autocomplete_words.push_back(assign_var1);
                }
            }

            // Save unit string to second variable if present (or just output definition)
            std::stringstream ss;
            if (!assign_var2.empty()) {
                // If unit variable requested, format assignment
                ss << assign_var1 << " = " << fmtNum(c_res.value) << "\n";
                ss << assign_var2 << " = \"" << c_res.unitStr << "\"";
            } else {
                g_last_has_num = true;
                g_last_num_val = c_res.value;
                g_last_prefix = assign_var1 + " = ";
                g_last_suffix = c_res.unitStr.empty() ? "" : (" (" + c_res.unitStr + ")");
                ss << g_last_prefix << fmtNum(c_res.value) << g_last_suffix;
            }
            return toJavaString(env, ss.str());
        }

        std::string resultStr;
        if (c_res.textOnly) {
            resultStr = c_res.unitStr;
        } else {
            const auto history_number = appendHistory(c_res.value);
            g_last_has_num = true;
            g_last_num_val = c_res.value;
            g_last_prefix = "e" + std::to_string(history_number) + " = ";
            g_last_suffix = c_res.unitStr.empty() ? "" : (" (" + c_res.unitStr + ")");
            resultStr = g_last_prefix + fmtNum(c_res.value) + g_last_suffix;
        }
        return toJavaString(env, resultStr);
    }

    // 2. Handle help / help()
    if (trimmed_expr == "help()" || trimmed_expr == "help" || trimmed_expr.rfind("help(", 0) == 0) {
        std::string help_res;
        preprocessHelp(trimmed_expr, help_res);
        return toJavaString(env, help_res);
    }

    // 2b. Handle run("script_name") or run(script_name) command
    if (trimmed_expr.rfind("run(", 0) == 0 && trimmed_expr.back() == ')') {
        std::string sname = trimmed_expr.substr(4, trimmed_expr.size() - 5);
        sname.erase(0, sname.find_first_not_of(" \t\"'"));
        sname.erase(sname.find_last_not_of(" \t\"'") + 1);

        auto it = user_named_scripts.find(sname);
        if (it != user_named_scripts.end()) {
            script_console_output.clear();
            runScript(it->second);
            std::ostringstream out;
            if (!script_console_output.empty()) {
                for (size_t i = 0; i < script_console_output.size(); ++i) {
                    out << script_console_output[i];
                    if (i + 1 < script_console_output.size()) out << "\n";
                }
            } else {
                out << "Done";
            }
            return toJavaString(env, out.str());
        }
        return toJavaString(env, ("ERROR: Script '" + sname + "' not found"));
    }

    // 3. Handle plot commands
    if (handlePlotCommands(expression, err, result)) {
        if (!err.empty()) {
            return toJavaString(env, ("ERROR: " + err));
        }
        return toJavaString(env, "Plot");
    }

    // 4. Handle multi-statement lines separated by ';' or function definitions
    std::vector<std::string> stmts = splitIntoStatements(expression);
    if (stmts.size() > 1 || (!stmts.empty() && (stmts[0].rfind("fn ", 0) == 0 || stmts[0].rfind("def ", 0) == 0 || stmts[0].rfind("function ", 0) == 0))) {
        double last_res = 0.0;
        bool last_is_def = false;
        for (size_t i = 0; i < stmts.size(); ++i) {
            const auto& stmt = stmts[i];
            if (stmt.rfind("fn ", 0) == 0 || stmt.rfind("def ", 0) == 0 || stmt.rfind("function ", 0) == 0) {
                size_t open_paren = stmt.find('(');
                size_t close_paren = stmt.find(')', open_paren);
                size_t open_brace = stmt.find('{', close_paren);
                if (open_paren != std::string::npos && close_paren != std::string::npos && open_brace != std::string::npos) {
                    size_t kw_len = (stmt.rfind("fn ", 0) == 0) ? 3 : ((stmt.rfind("def ", 0) == 0) ? 4 : 9);
                    std::string fname = stmt.substr(kw_len, open_paren - kw_len);
                    fname.erase(0, fname.find_first_not_of(" \t"));
                    fname.erase(fname.find_last_not_of(" \t") + 1);
                    if (isReservedName(fname)) return toJavaString(env, "ERROR: Reserved name: " + fname);

                    std::string params_str = stmt.substr(open_paren + 1, close_paren - open_paren - 1);
                    std::vector<std::string> params;
                    size_t p_pos = 0;
                    while (p_pos < params_str.size()) {
                        size_t comma = params_str.find(',', p_pos);
                        std::string p = (comma == std::string::npos) ? params_str.substr(p_pos) : params_str.substr(p_pos, comma - p_pos);
                        p.erase(0, p.find_first_not_of(" \t"));
                        p.erase(p.find_last_not_of(" \t") + 1);
                        if (!p.empty()) params.push_back(p);
                        if (comma == std::string::npos) break;
                        p_pos = comma + 1;
                    }
                    if (std::any_of(params.begin(), params.end(), isReservedName)) return toJavaString(env, "ERROR: Reserved parameter");

                    std::vector<std::string> body_stmts;
                    size_t j = i + 1;
                    while (j < stmts.size() && stmts[j] != "}") {
                        body_stmts.push_back(stmts[j]);
                        j++;
                    }
                    i = j;

                    user_script_funcs.erase(std::remove_if(user_script_funcs.begin(), user_script_funcs.end(), [&](const CustomScriptFunc& f){
                        return f.name == fname;
                    }), user_script_funcs.end());

                    user_script_funcs.push_back({fname, params, body_stmts});
                    resultStr = "Def OK";
                    last_is_def = true;
                }
            } else {
                last_res = evaluateInput(stmt, err, last_is_def);
                if (!err.empty()) break;
            }
        }

        if (!err.empty()) {
            return toJavaString(env, ("ERROR: " + err));
        } else {
            if (!last_is_def) {
                const auto history_number = appendHistory(last_res);
                g_last_has_num = true;
                g_last_num_val = last_res;
                g_last_prefix = "e" + std::to_string(history_number) + " = ";
                if (g_last_result_angle == AngleUnit::DEG) {
                    g_last_suffix = " (deg)";
                } else if (g_last_result_angle == AngleUnit::RAD) {
                    g_last_suffix = " (rad)";
                }
                resultStr = g_last_prefix + fmtNum(last_res) + g_last_suffix;
            }
            return toJavaString(env, resultStr);
        }
    }

    // 5. Standard single-statement evaluation
    result = evaluateInput(expression, err, isDef);
    if (!err.empty()) {
        return toJavaString(env, ("ERROR: " + err));
    }

    if (isDef) {
        size_t eq_pos = expression.find('=');
        std::string lhs = (eq_pos != std::string::npos) ? expression.substr(0, eq_pos) : expression;
        lhs.erase(0, lhs.find_first_not_of(" \t"));
        lhs.erase(lhs.find_last_not_of(" \t") + 1);
        if (lhs.rfind("const ", 0) == 0 || lhs.rfind("const\t", 0) == 0) {
            lhs = lhs.substr(5);
            lhs.erase(0, lhs.find_first_not_of(" \t"));
        }
        if (lhs.find('(') == std::string::npos) {
            if (user_arrays.find(lhs) != user_arrays.end()) {
                resultStr = lhs + " = Array[" + std::to_string(user_arrays[lhs].size()) + "]";
            } else {
                g_last_has_num = true;
                g_last_num_val = result;
                g_last_prefix = lhs + " = ";
                if (g_last_result_angle == AngleUnit::DEG) {
                    g_last_suffix = " (deg)";
                } else if (g_last_result_angle == AngleUnit::RAD) {
                    g_last_suffix = " (rad)";
                }
                resultStr = g_last_prefix + fmtNum(result) + g_last_suffix;
            }
        } else {
            resultStr = "Def OK";
        }
    } else if (g_last_eval_is_array) {
        std::string trimmed = expression;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
        std::string arr_str = "[";
        for (size_t k = 0; k < g_last_eval_array.size(); ++k) {
            arr_str += fmtNum(g_last_eval_array[k]);
            if (k + 1 < g_last_eval_array.size()) arr_str += ", ";
        }
        arr_str += "]";
        if (user_arrays.find(trimmed) != user_arrays.end()) {
            resultStr = trimmed + " = " + arr_str;
        } else {
            resultStr = arr_str;
        }
    } else {
        std::string trimmed = expression;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

        std::string label = "";
        for (const auto& cn : user_consts) {
            if (cn.name == trimmed) { label = trimmed; break; }
        }
        if (label.empty()) {
            for (const auto& arg : user_args) {
                if (arg.name == trimmed) { label = trimmed; break; }
            }
        }
        if (label.empty() && trimmed.size() > 1 && trimmed[0] == 'e' &&
            std::all_of(trimmed.begin() + 1, trimmed.end(), ::isdigit)) {
            label = trimmed;
        }

        g_last_has_num = true;
        g_last_num_val = result;
        if (g_last_result_angle == AngleUnit::DEG) {
            g_last_suffix = " (deg)";
        } else if (g_last_result_angle == AngleUnit::RAD) {
            g_last_suffix = " (rad)";
        }

        if (!label.empty()) {
            g_last_prefix = label + " = ";
            resultStr = g_last_prefix + fmtNum(result) + g_last_suffix;
        } else {
            const auto history_number = appendHistory(result);
            g_last_prefix = "e" + std::to_string(history_number) + " = ";
            resultStr = g_last_prefix + fmtNum(result) + g_last_suffix;
        }
    }

    return toJavaString(env, resultStr);
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return nullptr;
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeAutocomplete(JNIEnv *env, jobject thiz, jstring input_jstr, jint caret_pos, jintArray out_cursor) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    if (!input_jstr) return toJavaString(env, "");

    std::string text = toCppString(env, input_jstr);

    int cursor_pos = static_cast<int>(caret_pos);
    handleTabCompletion(text, cursor_pos);

    if (out_cursor != nullptr && env->GetArrayLength(out_cursor) > 0) {
        jint res_pos = static_cast<jint>(cursor_pos);
        env->SetIntArrayRegion(out_cursor, 0, 1, &res_pos);
    }

    return toJavaString(env, text);
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return nullptr;
}

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeResetAutocomplete(JNIEnv *, jobject) {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    resetTabState();
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeRunScript(JNIEnv *env, jobject thiz, jstring code_jstr) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    if (!code_jstr) return toJavaString(env, "");

    std::string code = toCppString(env, code_jstr);

    script_console_output.clear();
    runScript(code);

    std::ostringstream out;
    for (size_t i = 0; i < script_console_output.size(); ++i) {
        out << script_console_output[i];
        if (i + 1 < script_console_output.size()) out << "\n";
    }

    return toJavaString(env, out.str());
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return nullptr;
}

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeClearHistory(JNIEnv *env, jobject thiz) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    clearHistory();
    for (const auto& arg : user_args) removeUserAutocompleteWord(arg.name);
    for (const auto& array : user_arrays) removeUserAutocompleteWord(array.first);
    user_args.clear();
    user_arrays.clear();
    script_console_output.clear();
    resetTabState();
    LOGI("History and variables cleared.");
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return;
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetVariables(JNIEnv *env, jobject thiz) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    std::ostringstream out;
    out << std::setprecision(17);
    if (user_args.empty() && user_arrays.empty() && history.empty()) {
        out << "No user variables defined.";
    } else {
        for (size_t i = 0; i < history.size(); ++i) {
            out << "e" << (history_first_number + i) << " = " << history[i] << "\n";
        }
        for (const auto& arg : user_args) {
            out << arg.name << " = " << arg.val << "\n";
        }
        for (const auto& pair : user_arrays) {
            out << pair.first << " = [";
            for (size_t i = 0; i < pair.second.size(); ++i) {
                out << pair.second[i] << (i + 1 < pair.second.size() ? ", " : "");
            }
            out << "]\n";
        }
    }
    return toJavaString(env, out.str());
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return nullptr;
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetFormulas(JNIEnv *env, jobject thiz) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    std::ostringstream out;
    if (user_funcs.empty()) {
        out << "No custom formulas defined.";
    } else {
        for (const auto& fn : user_funcs) {
            out << fn << "\n";
        }
    }
    return toJavaString(env, out.str());
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return nullptr;
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetConstants(JNIEnv *env, jobject thiz) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    std::ostringstream out;
    out << std::setprecision(17);
    out << "pi = 3.141592653589793\n";
    out << "e  = 2.718281828459045\n";
    for (const auto& c : user_consts) {
        out << c.name << " = " << c.val << "\n";
    }
    return toJavaString(env, out.str());
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return nullptr;
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetHelp(JNIEnv *env, jobject thiz, jstring topic_jstr) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    std::string topic = toCppString(env, topic_jstr);

    std::string helpText;
    preprocessHelp(topic, helpText);
    if (helpText.empty()) {
        helpText = "Cardulator Math REPL\n"
                   "Supported: sin, cos, tan, log, ln, sqrt, abs, sum, prod, if, rUni, rNor\n"
                   "SI Prefixes: 1k (1000), 2.5M, 10u, 5p\n"
                   "History: e1, e2, e3...\n"
                   "Type function name for details e.g. help(sin)";
    }

    return toJavaString(env, helpText);
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return nullptr;
}

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeSetDegRad(JNIEnv *env, jobject thiz, jboolean is_deg) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    degreesMode = is_deg;
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return;
}

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeSetFormatSettings(JNIEnv *env, jobject thiz, jint mode, jint precision, jboolean group_digits) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    fmt_mode = mode;
    fmt_precision = precision;
    use_thousands_sep = group_digits;
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return;
}

JNIEXPORT jboolean JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetDegRad(JNIEnv *, jobject) {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    return degreesMode ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeDeleteVariable(JNIEnv *env, jobject thiz, jstring name_jstr) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    if (!name_jstr) return JNI_FALSE;
    std::string name = toCppString(env, name_jstr);

    auto it = std::remove_if(user_args.begin(), user_args.end(), [&](const UserArg& a) { return a.name == name; });
    if (it != user_args.end()) {
        user_args.erase(it, user_args.end());
        removeUserAutocompleteWord(name);
        resetTabState();
        return JNI_TRUE;
    }
    if (user_arrays.find(name) != user_arrays.end()) {
        user_arrays.erase(name);
        removeUserAutocompleteWord(name);
        resetTabState();
        return JNI_TRUE;
    }
    return JNI_FALSE;
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return 0;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return 0;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return 0;
}

JNIEXPORT jboolean JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeDeleteConstant(JNIEnv *env, jobject thiz, jstring name_jstr) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    if (!name_jstr) return JNI_FALSE;
    std::string name = toCppString(env, name_jstr);

    auto it = std::remove_if(user_consts.begin(), user_consts.end(), [&](const UserConst& c) { return c.name == name; });
    if (it != user_consts.end()) {
        user_consts.erase(it, user_consts.end());
        removeUserAutocompleteWord(name);
        resetTabState();
        return JNI_TRUE;
    }
    return JNI_FALSE;
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return 0;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return 0;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return 0;
}

JNIEXPORT jboolean JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeDeleteFormula(JNIEnv *env, jobject thiz, jstring name_jstr) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    if (!name_jstr) return JNI_FALSE;
    std::string name = toCppString(env, name_jstr);

    auto it = std::remove_if(user_funcs.begin(), user_funcs.end(), [&](const std::string& fn) {
        size_t open = fn.find('(');
        std::string fname = (open != std::string::npos) ? fn.substr(0, open) : fn;
        fname.erase(0, fname.find_first_not_of(" \t"));
        fname.erase(fname.find_last_not_of(" \t") + 1);
        return fname == name;
    });
    if (it != user_funcs.end()) {
        user_funcs.erase(it, user_funcs.end());
        removeUserAutocompleteWord(name + "(");
        resetTabState();
        return JNI_TRUE;
    }
    return JNI_FALSE;
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return 0;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return 0;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return 0;
}

JNIEXPORT jobjectArray JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetPlotLines(JNIEnv *env, jobject) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    jclass cls = env->FindClass("com/cardputer/cardulator/NativePlotLine");
    if (!cls) return nullptr;
    jmethodID constructor = env->GetMethodID(cls, "<init>", "([D[DILjava/lang/String;)V");
    if (!constructor) return nullptr;
    jobjectArray lines = env->NewObjectArray(static_cast<jsize>(plot_lines.size()), cls, nullptr);
    if (!lines) return nullptr;
    for (size_t i = 0; i < plot_lines.size(); ++i) {
        const auto& line = plot_lines[i];
        if (line.x.size() != line.y.size() || line.x.size() > 100000) {
            throwJavaException(env, "java/lang/IllegalStateException", "Invalid plot size"); return nullptr;
        }
        const jsize size = static_cast<jsize>(line.x.size());
        jdoubleArray x = env->NewDoubleArray(size);
        jdoubleArray y = env->NewDoubleArray(size);
        if (!x || !y) return nullptr;
        if (size) {
            env->SetDoubleArrayRegion(x, 0, size, line.x.data());
            env->SetDoubleArrayRegion(y, 0, size, line.y.data());
        }
        jstring style = toJavaString(env, line.linestyle);
        if (!style || env->ExceptionCheck()) return nullptr;
        jobject object = env->NewObject(cls, constructor, x, y, static_cast<jint>(line.color), style);
        if (!object) return nullptr;
        env->SetObjectArrayElement(lines, static_cast<jsize>(i), object);
        env->DeleteLocalRef(object); env->DeleteLocalRef(style);
        env->DeleteLocalRef(x); env->DeleteLocalRef(y);
        if (env->ExceptionCheck()) return nullptr;
    }
    return lines;
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unable to get plot data"); return nullptr;
}

JNIEXPORT jdoubleArray JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetPlotLimits(JNIEnv *env, jobject) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    const jdouble limits[] = {plot_manual_limits ? 1.0 : 0.0, plot_xlim_min, plot_xlim_max, plot_ylim_min, plot_ylim_max};
    jdoubleArray result = env->NewDoubleArray(5);
    if (result) env->SetDoubleArrayRegion(result, 0, 5, limits);
    return result;
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unable to get plot limits"); return nullptr;
}

// Register a named script so it can be called from run(name)
JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeRegisterScript(JNIEnv *env, jobject thiz, jstring name_jstr, jstring code_jstr) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    std::string name = toCppString(env, name_jstr);
    std::string code = toCppString(env, code_jstr);
    if (name.empty()) return;
    user_named_scripts[name] = code;
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return;
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeFormatNumber(JNIEnv *env, jobject thiz, jdouble val) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    return toJavaString(env, fmtNum(static_cast<double>(val)));
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return nullptr;
}

JNIEXPORT jboolean JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeHasLastNumeric(JNIEnv *env, jobject thiz) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    return g_last_has_num ? JNI_TRUE : JNI_FALSE;
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return 0;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return 0;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return 0;
}

JNIEXPORT jdouble JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetLastNumericVal(JNIEnv *env, jobject thiz) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    return static_cast<jdouble>(g_last_num_val);
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return 0;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return 0;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return 0;
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetLastNumericPrefix(JNIEnv *env, jobject thiz) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    return toJavaString(env, g_last_prefix);
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return nullptr;
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetLastNumericSuffix(JNIEnv *env, jobject thiz) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    return toJavaString(env, g_last_suffix);
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return nullptr;
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetConvUnits(JNIEnv *env, jobject thiz, jstring from_jstr, jstring prefix_jstr) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    std::string from_unit = toCppString(env, from_jstr);
    std::string prefix = toCppString(env, prefix_jstr);
    auto matches = getConvUnitMatches(from_unit, prefix);
    std::ostringstream oss;
    for (size_t i = 0; i < matches.size(); ++i) {
        if (i > 0) oss << "\n";
        oss << matches[i];
    }
    return toJavaString(env, oss.str());
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (const std::exception&) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Unknown native engine error"); return nullptr;
}

// The first two fields are name and parameter count; the remainder holds
// parameters followed by statement strings. No source-code reparsing is needed.
JNIEXPORT jobjectArray JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetScriptFunctions(JNIEnv *env, jobject) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    jclass stringClass = env->FindClass("java/lang/String");
    jclass arrayClass = env->FindClass("[Ljava/lang/String;");
    if (!stringClass || !arrayClass) return nullptr;
    jobjectArray functions = env->NewObjectArray(static_cast<jsize>(user_script_funcs.size()), arrayClass, nullptr);
    if (!functions) return nullptr;
    for (size_t i = 0; i < user_script_funcs.size(); ++i) {
        const auto& fn = user_script_funcs[i];
        const auto count = 2 + fn.params.size() + fn.statements.size();
        jobjectArray fields = env->NewObjectArray(static_cast<jsize>(count), stringClass, nullptr);
        if (!fields) return nullptr;
        std::vector<std::string> values{fn.name, std::to_string(fn.params.size())};
        values.insert(values.end(), fn.params.begin(), fn.params.end());
        values.insert(values.end(), fn.statements.begin(), fn.statements.end());
        for (size_t j = 0; j < values.size(); ++j) {
            jstring value = toJavaString(env, values[j]);
            if (!value) return nullptr;
            env->SetObjectArrayElement(fields, static_cast<jsize>(j), value);
            env->DeleteLocalRef(value);
        }
        env->SetObjectArrayElement(functions, static_cast<jsize>(i), fields);
        env->DeleteLocalRef(fields);
    }
    return functions;
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory"); return nullptr;
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Native engine error"); return nullptr;
}

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeRestoreScriptFunction(JNIEnv *env, jobject, jobjectArray fields) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    if (!fields || env->GetArrayLength(fields) < 2) return;
    const jsize size = env->GetArrayLength(fields);
    std::vector<std::string> values;
    for (jsize i = 0; i < size; ++i) {
        auto value = static_cast<jstring>(env->GetObjectArrayElement(fields, i));
        if (!value) return;
        values.push_back(toCppString(env, value));
        env->DeleteLocalRef(value);
    }
    size_t used = 0;
    const auto count = std::stoul(values[1], &used);
    if (used != values[1].size() || count > values.size() - 2 || values[0].empty() || isReservedName(values[0])) return;
    CustomScriptFunc fn{values[0], {}, {}};
    fn.params.assign(values.begin() + 2, values.begin() + 2 + count);
    if (std::any_of(fn.params.begin(), fn.params.end(), isReservedName)) return;
    fn.statements.assign(values.begin() + 2 + count, values.end());
    user_script_funcs.erase(std::remove_if(user_script_funcs.begin(), user_script_funcs.end(),
        [&](const CustomScriptFunc& existing) { return existing.name == fn.name; }), user_script_funcs.end());
    user_script_funcs.push_back(std::move(fn));
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory");
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Invalid saved script function");
}

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeRestoreScalar(JNIEnv *env, jobject, jstring nameValue, jdouble value, jboolean isConstant) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    const std::string name = toCppString(env, nameValue);
    if (name.empty() || isReservedName(name)) return;
    if (isConstant) {
        auto it = std::find_if(user_consts.begin(), user_consts.end(), [&](const UserConst& item) { return item.name == name; });
        if (it == user_consts.end()) user_consts.push_back({name, value}); else it->val = value;
    } else {
        auto it = std::find_if(user_args.begin(), user_args.end(), [&](const UserArg& item) { return item.name == name; });
        if (it == user_args.end()) user_args.push_back({name, value}); else it->val = value;
    }
    if (std::find(autocomplete_words.begin(), autocomplete_words.end(), name) == autocomplete_words.end()) autocomplete_words.push_back(name);
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory");
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Invalid saved variable");
}

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeRestoreArray(JNIEnv *env, jobject, jstring nameValue, jdoubleArray values) try {
    std::lock_guard<std::recursive_mutex> engineLock(g_engine_mutex);
    const std::string name = toCppString(env, nameValue);
    if (name.empty() || isReservedName(name) || !values) return;
    const jsize count = env->GetArrayLength(values);
    std::vector<double> restored(static_cast<size_t>(count));
    if (count) env->GetDoubleArrayRegion(values, 0, count, restored.data());
    if (env->ExceptionCheck()) return;
    user_arrays[name] = std::move(restored);
    if (std::find(autocomplete_words.begin(), autocomplete_words.end(), name) == autocomplete_words.end()) autocomplete_words.push_back(name);
} catch (const std::bad_alloc&) {
    throwJavaException(env, "java/lang/OutOfMemoryError", "Native engine ran out of memory");
} catch (...) {
    throwJavaException(env, "java/lang/RuntimeException", "Invalid saved array");
}

} // extern "C"
