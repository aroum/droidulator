#include <jni.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <android/log.h>

#define LOG_TAG "CardulatorJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define UNIT_TEST 1

#include "app_state.h"
#include "help_engine.h"
#include "math_funcs.h"
#include "plot_engine.h"
#include "script_engine.h"
#include "math_engine.h"
#include "units_adapter.h"


extern "C" {

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeInit(JNIEnv *env, jobject thiz) {
    history.clear();
    user_args.clear();
    user_funcs.clear();
    user_script_funcs.clear();
    user_consts.clear();
    user_arrays.clear();
    script_console_output.clear();
    degreesMode = true;
    LOGI("Cardulator Native Engine Initialized.");
}

static inline std::string toCppString(JNIEnv *env, jstring jstr) {
    if (!jstr) return "";
    const char *cstr = env->GetStringUTFChars(jstr, nullptr);
    std::string s(cstr ? cstr : "");
    env->ReleaseStringUTFChars(jstr, cstr);
    return s;
}

static std::string g_units_dat_path;

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeInitUnits(JNIEnv *env, jobject thiz, jstring path_jstr) {
    g_units_dat_path = toCppString(env, path_jstr);
    bridge_init(g_units_dat_path.empty() ? nullptr : g_units_dat_path.c_str());
    LOGI("GNU Units initialised: %s", g_units_dat_path.c_str());
}

// Preview evaluation — evaluates formula without adding to history (used for live formula dialog preview)
JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeEvaluatePreview(JNIEnv *env, jobject thiz, jstring input_jstr) {
    std::string expression = toCppString(env, input_jstr);
    if (expression.empty()) return env->NewStringUTF("");

    std::string err;
    double val = evaluateWithoutHistory(expression, err);
    if (!err.empty()) return env->NewStringUTF(("ERROR: " + err).c_str());
    return env->NewStringUTF(fmtNum(val).c_str());
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeEvaluate(JNIEnv *env, jobject thiz, jstring input_jstr) {
    std::string expression = toCppString(env, input_jstr);
    if (expression.empty()) return env->NewStringUTF("");

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
            return env->NewStringUTF(("ERROR: " + err).c_str());
        }
        return env->NewStringUTF(print_res.c_str());
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
            return env->NewStringUTF("ERROR: conv requires 2 or 3 arguments: conv(val, \"unit_a\", [\"unit_b\"])");
        }

        std::string eval_err;
        double val = evaluate(conv_args[0], eval_err);
        std::string unit_a = conv_args[1];
        std::string unit_b = (conv_args.size() == 3) ? conv_args[2] : "";

        if (!eval_err.empty() || std::isnan(val)) {
            val = 0.0;
            unit_a = conv_args[0];
            unit_b = (conv_args.size() >= 2) ? conv_args[1] : "";
        }

        ConvResult c_res = handleConv(val, unit_a, unit_b);
        if (!c_res.success) {
            return env->NewStringUTF(("ERROR: " + c_res.errorMsg).c_str());
        }

        if (is_assign) {
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
            }

            // Save unit string to second variable if present (or just output definition)
            std::stringstream ss;
            if (!assign_var2.empty()) {
                // If unit variable requested, format assignment
                ss << assign_var1 << " = " << fmtNum(c_res.value) << "\n";
                ss << assign_var2 << " = \"" << c_res.unitStr << "\"";
            } else {
                ss << assign_var1 << " = " << fmtNum(c_res.value);
                if (!c_res.unitStr.empty()) {
                    ss << " (" << c_res.unitStr << ")";
                }
            }
            return env->NewStringUTF(ss.str().c_str());
        }

        std::string resultStr;
        if (c_res.value == 0.0 && !c_res.unitStr.empty() && (std::isdigit(c_res.unitStr[0]) || c_res.unitStr.find('R') != std::string::npos)) {
            resultStr = c_res.unitStr;
        } else {
            history.push_back(c_res.value);
            resultStr = "e" + std::to_string(history.size()) + " = " + fmtNum(c_res.value);
            if (!c_res.unitStr.empty()) {
                resultStr += " (" + c_res.unitStr + ")";
            }
        }
        return env->NewStringUTF(resultStr.c_str());
    }

    // 2. Handle help / help()
    if (trimmed_expr == "help()" || trimmed_expr == "help" || trimmed_expr.rfind("help(", 0) == 0) {
        std::string help_res;
        preprocessHelp(trimmed_expr, help_res);
        return env->NewStringUTF(help_res.c_str());
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
            return env->NewStringUTF(out.str().c_str());
        }
        return env->NewStringUTF(("ERROR: Script '" + sname + "' not found").c_str());
    }

    // 3. Handle plot commands
    if (handlePlotCommands(expression, err, result)) {
        if (!err.empty()) {
            return env->NewStringUTF(("ERROR: " + err).c_str());
        }
        return env->NewStringUTF("Plot");
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
            return env->NewStringUTF(("ERROR: " + err).c_str());
        } else {
            if (!last_is_def) {
                history.push_back(last_res);
                resultStr = "e" + std::to_string(history.size()) + " = " + fmtNum(last_res);
            }
            return env->NewStringUTF(resultStr.c_str());
        }
    }

    // 5. Standard single-statement evaluation
    result = evaluateInput(expression, err, isDef);
    if (!err.empty()) {
        return env->NewStringUTF(("ERROR: " + err).c_str());
    }

    if (isDef) {
        size_t eq_pos = expression.find('=');
        std::string lhs = (eq_pos != std::string::npos) ? expression.substr(0, eq_pos) : expression;
        lhs.erase(0, lhs.find_first_not_of(" \t"));
        lhs.erase(lhs.find_last_not_of(" \t") + 1);
        if (lhs.find('(') == std::string::npos) {
            if (user_arrays.find(lhs) != user_arrays.end()) {
                resultStr = lhs + " = Array[" + std::to_string(user_arrays[lhs].size()) + "]";
            } else {
                resultStr = lhs + " = " + fmtNum(result);
            }
        } else {
            resultStr = "Def OK";
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

        if (!label.empty()) {
            resultStr = label + " = " + fmtNum(result);
        } else {
            history.push_back(result);
            resultStr = "e" + std::to_string(history.size()) + " = " + fmtNum(result);
        }
    }

    return env->NewStringUTF(resultStr.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeAutocomplete(JNIEnv *env, jobject thiz, jstring input_jstr, jint caret_pos, jintArray out_cursor) {
    if (!input_jstr) return env->NewStringUTF("");

    const char *input_cstr = env->GetStringUTFChars(input_jstr, nullptr);
    std::string text(input_cstr ? input_cstr : "");
    env->ReleaseStringUTFChars(input_jstr, input_cstr);

    int cursor_pos = static_cast<int>(caret_pos);
    handleTabCompletion(text, cursor_pos);

    if (out_cursor != nullptr && env->GetArrayLength(out_cursor) > 0) {
        jint res_pos = static_cast<jint>(cursor_pos);
        env->SetIntArrayRegion(out_cursor, 0, 1, &res_pos);
    }

    return env->NewStringUTF(text.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeRunScript(JNIEnv *env, jobject thiz, jstring code_jstr) {
    if (!code_jstr) return env->NewStringUTF("");

    const char *code_cstr = env->GetStringUTFChars(code_jstr, nullptr);
    std::string code(code_cstr ? code_cstr : "");
    env->ReleaseStringUTFChars(code_jstr, code_cstr);

    script_console_output.clear();
    runScript(code);

    std::ostringstream out;
    for (size_t i = 0; i < script_console_output.size(); ++i) {
        out << script_console_output[i];
        if (i + 1 < script_console_output.size()) out << "\n";
    }

    return env->NewStringUTF(out.str().c_str());
}

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeClearHistory(JNIEnv *env, jobject thiz) {
    history.clear();
    user_args.clear();
    user_funcs.clear();
    user_script_funcs.clear();
    user_consts.clear();
    user_arrays.clear();
    script_console_output.clear();
    LOGI("History and session state cleared.");
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetVariables(JNIEnv *env, jobject thiz) {
    std::ostringstream out;
    if (user_args.empty() && user_arrays.empty() && history.empty()) {
        out << "No user variables defined.";
    } else {
        for (size_t i = 0; i < history.size(); ++i) {
            out << "e" << (i + 1) << " = " << history[i] << "\n";
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
    return env->NewStringUTF(out.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetFormulas(JNIEnv *env, jobject thiz) {
    std::ostringstream out;
    if (user_funcs.empty()) {
        out << "No custom formulas defined.";
    } else {
        for (const auto& fn : user_funcs) {
            out << fn << "\n";
        }
    }
    return env->NewStringUTF(out.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetConstants(JNIEnv *env, jobject thiz) {
    std::ostringstream out;
    out << "pi = 3.141592653589793\n";
    out << "e  = 2.718281828459045\n";
    for (const auto& c : user_consts) {
        out << c.name << " = " << c.val << "\n";
    }
    return env->NewStringUTF(out.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetHelp(JNIEnv *env, jobject thiz, jstring topic_jstr) {
    std::string topic;
    if (topic_jstr) {
        const char *cstr = env->GetStringUTFChars(topic_jstr, nullptr);
        if (cstr) {
            topic = cstr;
            env->ReleaseStringUTFChars(topic_jstr, cstr);
        }
    }

    std::string helpText;
    preprocessHelp(topic, helpText);
    if (helpText.empty()) {
        helpText = "Cardulator Math REPL\n"
                   "Supported: sin, cos, tan, log, ln, sqrt, abs, sum, prod, if, rUni, rNor\n"
                   "SI Prefixes: 1k (1000), 2.5M, 10u, 5p\n"
                   "History: e1, e2, e3...\n"
                   "Type function name for details e.g. help(sin)";
    }

    return env->NewStringUTF(helpText.c_str());
}

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeSetDegRad(JNIEnv *env, jobject thiz, jboolean is_deg) {
    degreesMode = is_deg;
}

JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeSetFormatSettings(JNIEnv *env, jobject thiz, jint mode, jint precision, jboolean group_digits) {
    fmt_mode = mode;
    fmt_precision = precision;
    use_thousands_sep = group_digits;
}

JNIEXPORT jboolean JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeDeleteVariable(JNIEnv *env, jobject thiz, jstring name_jstr) {
    if (!name_jstr) return JNI_FALSE;
    const char *name_cstr = env->GetStringUTFChars(name_jstr, nullptr);
    std::string name(name_cstr ? name_cstr : "");
    env->ReleaseStringUTFChars(name_jstr, name_cstr);

    auto it = std::remove_if(user_args.begin(), user_args.end(), [&](const UserArg& a) { return a.name == name; });
    if (it != user_args.end()) {
        user_args.erase(it, user_args.end());
        return JNI_TRUE;
    }
    if (user_arrays.find(name) != user_arrays.end()) {
        user_arrays.erase(name);
        return JNI_TRUE;
    }
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeDeleteConstant(JNIEnv *env, jobject thiz, jstring name_jstr) {
    if (!name_jstr) return JNI_FALSE;
    const char *name_cstr = env->GetStringUTFChars(name_jstr, nullptr);
    std::string name(name_cstr ? name_cstr : "");
    env->ReleaseStringUTFChars(name_jstr, name_cstr);

    auto it = std::remove_if(user_consts.begin(), user_consts.end(), [&](const UserConst& c) { return c.name == name; });
    if (it != user_consts.end()) {
        user_consts.erase(it, user_consts.end());
        return JNI_TRUE;
    }
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeDeleteFormula(JNIEnv *env, jobject thiz, jstring name_jstr) {
    if (!name_jstr) return JNI_FALSE;
    const char *name_cstr = env->GetStringUTFChars(name_jstr, nullptr);
    std::string name(name_cstr ? name_cstr : "");
    env->ReleaseStringUTFChars(name_jstr, name_cstr);

    auto it = std::remove_if(user_funcs.begin(), user_funcs.end(), [&](const std::string& fn) {
        size_t open = fn.find('(');
        std::string fname = (open != std::string::npos) ? fn.substr(0, open) : fn;
        fname.erase(0, fname.find_first_not_of(" \t"));
        fname.erase(fname.find_last_not_of(" \t") + 1);
        return fname == name;
    });
    if (it != user_funcs.end()) {
        user_funcs.erase(it, user_funcs.end());
        return JNI_TRUE;
    }
    return JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeGetPlotLines(JNIEnv *env, jobject thiz) {
    std::stringstream ss;
    ss << plot_lines.size() << "\n";
    ss << (plot_manual_limits ? 1 : 0) << "\n";
    ss << plot_xlim_min << " " << plot_xlim_max << " " << plot_ylim_min << " " << plot_ylim_max << "\n";
    for (const auto& line : plot_lines) {
        ss << line.color << ";" << line.linestyle << ";" << line.x.size() << "\n";
        for (size_t i = 0; i < line.x.size(); ++i) {
            if (i > 0) ss << " ";
            ss << line.x[i];
        }
        ss << "\n";
        for (size_t i = 0; i < line.y.size(); ++i) {
            if (i > 0) ss << " ";
            ss << line.y[i];
        }
        ss << "\n";
    }
    return env->NewStringUTF(ss.str().c_str());
}

// Register a named script so it can be called from run(name)
JNIEXPORT void JNICALL
Java_com_cardputer_cardulator_CardulatorEngine_nativeRegisterScript(JNIEnv *env, jobject thiz, jstring name_jstr, jstring code_jstr) {
    std::string name = toCppString(env, name_jstr);
    std::string code = toCppString(env, code_jstr);
    if (name.empty()) return;
    user_named_scripts[name] = code;
}

} // extern "C"
