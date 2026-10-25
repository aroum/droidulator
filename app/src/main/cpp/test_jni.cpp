#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <cmath>

#define UNIT_TEST 1

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
    std::cout << "[Droidulator Native Test] Initializing..." << std::endl;
    std::string err;
    bool isDef = false;

    // 1. Basic math test
    double res = evaluateInput("2 + 3 * 4", err, isDef);
    std::cout << "2 + 3 * 4 = " << res << std::endl;
    assert(res == 14.0);

    // 2. Engineering suffix test
    double res2 = evaluateInput("1k + 500", err, isDef);
    std::cout << "1k + 500 = " << res2 << std::endl;
    assert(res2 == 1500.0);

    // 3. Units adapter test (conversion to explicit target unit)
    //    The path is resolved relative to the binary's working dir.
    std::string units_dat = std::string(UNITS_DAT_PATH);
    ConvResult c1 = handleConv(5, "mile", "km", units_dat);
    std::cout << "conv(5, \"mile\", \"km\") = " << c1.value << " " << c1.unitStr << std::endl;
    assert(c1.success);
    assert(std::abs(c1.value - 8.04672) < 0.0001);
    assert(c1.unitStr == "km");

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
    use_thousands_sep = true;

    // NORM mode (Mode 0)
    fmt_mode = 0;
    fmt_precision = 6;
    std::string s_norm_std = fmtNum(1234.567);
    std::cout << "NORM std: " << s_norm_std << std::endl;
    assert(s_norm_std == "1 234.57");

    std::string s_norm_large = fmtNum(12345678.9); // >= 1e7 => sci
    std::cout << "NORM large: " << s_norm_large << std::endl;
    assert(s_norm_large.find("e+") != std::string::npos);

    // FLT mode (Mode 1: Pure float, no exponent, strips trailing zeros)
    fmt_mode = 1;
    fmt_precision = 6;
    std::string s_flt = fmtNum(1234.500);
    std::cout << "FLT 6dp + spaces: " << s_flt << std::endl;
    assert(s_flt == "1 234.5");

    std::string s_flt_small = fmtNum(0.0000012345);
    std::cout << "FLT small: " << s_flt_small << std::endl;
    assert(s_flt_small == "0.000 001");

    // FIX mode (Mode 2: Fixed point, preserves trailing zeros)
    fmt_mode = 2;
    fmt_precision = 6;
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
    fmt_mode = 3;
    fmt_precision = 4;
    std::string s_sci = fmtNum(0.0000012345);
    std::cout << "SCI 4dp + spaces: " << s_sci << std::endl;
    assert(s_sci == "1.234 5e-06");

    // ENG mode (Mode 4: Engineering exponent multiple of 3)
    fmt_mode = 4;
    fmt_precision = 3;
    std::string s_eng1 = fmtNum(123456.789);
    std::cout << "ENG 3dp + spaces (123k): " << s_eng1 << std::endl;
    assert(s_eng1 == "123.457e+03");

    std::string s_eng2 = fmtNum(0.0000012345);
    std::cout << "ENG 3dp + spaces (u): " << s_eng2 << std::endl;
    assert(s_eng2 == "1.235e-06");

    // Test without digit grouping
    use_thousands_sep = false;
    fmt_mode = 2; // FIX
    fmt_precision = 6;
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
    auto_brackets = true;
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

    // Test Case 5: User variable & constant priority (cycling "para" -> "power")
    reset_tab();
    user_args.clear();
    user_consts.clear();
    user_consts.push_back({"para", 3.14});
    user_args.push_back({"power", 42.0});

    std::string expr5 = "pa";
    int cursor5 = 2;
    handleTabCompletion(expr5, cursor5);
    std::cout << "Test 5.1 [pa -> para]: " << expr5 << " cursor=" << cursor5 << std::endl;
    assert(expr5 == "para" && cursor5 == 4);

    handleTabCompletion(expr5, cursor5);
    std::cout << "Test 5.2 [repeat TAB on single match para -> para]: " << expr5 << " cursor=" << cursor5 << std::endl;
    assert(expr5 == "para" && cursor5 == 4);

    // Clear user variables for clean subsequent tests
    user_args.clear();
    user_consts.clear();

    // Test Case 6: Autocomplete with auto_brackets = false
    reset_tab();
    auto_brackets = false;
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
    auto_brackets = true;
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
    assert(handled && plot_err.empty() && plot_manual_limits && plot_xlim_min == -5.0 && plot_xlim_max == 5.0);

    handled = handlePlotCommands("plot.ylim([-10 10])", plot_err, plot_res);
    assert(handled && plot_err.empty() && plot_manual_limits && plot_ylim_min == -10.0 && plot_ylim_max == 10.0);

    handled = handlePlotCommands("plot.xlim(-2, 2)", plot_err, plot_res);
    assert(handled && plot_err.empty() && plot_manual_limits && plot_xlim_min == -2.0 && plot_xlim_max == 2.0);

    // Test Case 8: conv(...) units autocomplete (mm, mmHg, mm^2, mm2, std_mm2)
    reset_tab();
    auto_brackets = true;
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
    degreesMode = true;
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
    degreesMode = false;
    double t_sin_rad = evaluate("sin(pi / 6)", err);
    assert(err.empty() && std::abs(t_sin_rad - 0.5) < 1e-6);

    double t_asin_rad = evaluate("asin(1)", err);
    assert(err.empty() && std::abs(t_asin_rad - M_PI / 2.0) < 1e-6);

    double t_atan2_rad = evaluate("atan2(1, 1)", err);
    assert(err.empty() && std::abs(t_atan2_rad - M_PI / 4.0) < 1e-6);
    degreesMode = true;

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

    // Test Case 11: Acceleration conversions
    ConvResult c_acc1 = handleConv(1, "m/s^2", "cm/s^2", units_dat);
    assert(c_acc1.success && std::abs(c_acc1.value - 100.0) < 1e-3);

    ConvResult c_acc2 = handleConv(1, "g0", "m/s^2", units_dat);
    assert(c_acc2.success && std::abs(c_acc2.value - 9.80665) < 1e-3);

    ConvResult c_acc3 = handleConv(9.80665, "m/s^2", "g", units_dat);
    assert(c_acc3.success && std::abs(c_acc3.value - 1.0) < 1e-3);

    ConvResult c_acc4 = handleConv(100, "gal", "m/s^2", units_dat);
    assert(c_acc4.success && std::abs(c_acc4.value - 1.0) < 1e-3);

    ConvResult c_acc5 = handleConv(1, "gravity", "ft/s^2", units_dat);
    assert(c_acc5.success && std::abs(c_acc5.value - 32.174) < 0.05);

    // Test Case 12: Acceleration dimension-aware tab completion
    reset_tab();
    std::string expr_acc = "conv(10, \"m/s^2\", c";
    int cursor_acc = 19;
    handleTabCompletion(expr_acc, cursor_acc);
    assert(expr_acc == "conv(10, \"m/s^2\", cm/s^2" && cursor_acc == 24);

    // Test Case 13: Angle operations and output formatting
    degreesMode = true;
    double val_asin = evaluateInput("asin(0.5)", err, isDef);
    assert(err.empty() && std::abs(val_asin - 30.0) < 1e-6);
    assert(g_last_result_angle == AngleUnit::DEG);

    degreesMode = false;
    double val_asin_rad = evaluateInput("asin(0.5)", err, isDef);
    assert(err.empty() && std::abs(val_asin_rad - 0.5235987) < 1e-5);
    assert(g_last_result_angle == AngleUnit::RAD);

    // Variable assignment x = asin(0.5)
    degreesMode = true;
    evaluateInput("x_angle = asin(0.5)", err, isDef);
    assert(err.empty() && isDef);
    assert(g_last_result_angle == AngleUnit::DEG);

    // Conversions deg2rad and rad2deg
    evaluateInput("r_angle = deg2rad(180)", err, isDef);
    assert(err.empty() && isDef);
    assert(g_last_result_angle == AngleUnit::RAD);

    evaluateInput("d_angle = rad2deg(pi)", err, isDef);
    assert(err.empty() && isDef);
    assert(g_last_result_angle == AngleUnit::DEG);

    // Non-angle trig operations must reset g_last_result_angle to NONE
    evaluateInput("sin(30)", err, isDef);
    assert(g_last_result_angle == AngleUnit::NONE);

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

    std::cout << "[Droidulator Native Test] ALL TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
