#include "help_engine.h"

bool preprocessHelp(const std::string& s, std::string& help_out) {
    std::string trimmed = s;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    if (trimmed == "help()" || trimmed == "help" || trimmed == "help( )") {
        help_out = "=== Droidulator Help ===\n"
                   "Trig: sin, cos, tan, cot, sec, csc, tg, ctg\n"
                   "Arc-Trig: asin, acos, atan, acot, asec, acsc\n"
                   "Hyperbolic: sinh, cosh, tanh, coth, sech, csch\n"
                   "Arc-Hyp: asinh, acosh, atanh, acoth, asech, acsch\n"
                   "Math: sqrt, cbrt, exp, ln, log, log2, logb, abs, mod\n"
                   "Round: ceil, floor, round, trunc, sgn\n"
                   "Stats: mean, median, var, std, rUni, rNor\n"
                   "Combinatorics: fact, C, P, gcd, lcm, fib\n"
                   "Units & System: conv, len, print, plot, help\n"
                   "Tip: type help(name) e.g. help(trig) or help(errors)";
        return true;
    }
    if (trimmed.rfind("help(", 0) == 0 && trimmed.back() == ')') {
        std::string target = trimmed.substr(5, trimmed.size() - 6);
        target.erase(0, target.find_first_not_of(" \t\"'"));
        target.erase(target.find_last_not_of(" \t\"'") + 1);
        if (target == "plot") {
            help_out = "=== plot() Help ===\n"
                       "Usage: plot(y) or plot(x, y, [col], [style])\n"
                       "Colors (col):\n"
                       "  'r' (red), 'g' (green), 'b' (blue)\n"
                       "  'c' (cyan), 'm' (magenta), 'y' (yellow)\n"
                       "  'k' (black), 'w' (white)\n"
                       "Styles (style):\n"
                       "  '-'  (solid line)\n"
                       "  '--' (dashed line)\n"
                       "  '-.' (dash-dot line)\n"
                       "  ':'  (dotted line)\n"
                       "Commands:\n"
                       "  plot.hold(1) / plot.hold(0) : hold overlay\n"
                       "  plot.xlim(min, max) / plot.ylim(min, max)\n"
                       "  plot.show()  : switch to plot screen\n"
                       "  plot.close() : reset plots";
        }
        else if (target == "print") {
            help_out = "=== print() Help ===\n"
                       "print(expr) : evaluate & output expr\n"
                       "print(\"text {var}\") : formatted string with var";
        }
        else if (target == "conv" || target == "units") {
            help_out = "=== conv() / Units Conversion Help ===\n"
                       "Syntax:\n"
                       "  conv(val, \"unit_a\", [\"unit_b\"])\n\n"
                       "Examples:\n"
                       "  conv(100, \"km/hr\", \"m/s\") -> 27.777778 m/s\n"
                       "  conv(30, \"dBm\", \"mW\")     -> 1000 mW\n"
                       "  conv(20, \"dB\", \"times\")   -> 100 times\n"
                       "  conv(24, \"AWG\", \"mm^2\")   -> 0.20473 mm^2\n\n"
                       "Component markings must be quoted: conv(\"104\", \"pf\") decodes a capacitor marking;\n"
                       "conv(104, \"pf\") means the numeric value 104 pF.\n\n"
                       "Supported Base Units by Category:\n"
                       "  - Length: m, mile, nmi (nauticalmile), inch (in), ft, yd, fathom, cable, marineleague, angstrom, lightyear, parsec, au\n"
                       "  - Mass: g, lb, oz, ton, tonne, grain, dram, stone, slug, amu\n"
                       "  - Time: s, min, hr, day, week, month, year\n"
                       "  - Speed: m/s, km/hr, mph, knot, brknot, mach, c (speed of light)\n"
                       "  - Area: m^2, sqft, sqin, hectare (ha), acre, barn\n"
                       "  - Volume: m^3, liter (l, L), gallon (gal), quart (qt), pint (pt), cup, fluidounce (floz), tablespoon (tbsp), teaspoon (tsp), barrel (bbl)\n"
                       "  - Pressure: Pa, bar, atm, psi, torr, mmHg, inHg\n"
                       "  - Energy/Work: J, cal, BTU, eV, kWh, erg, ftlb\n"
                       "  - Power: W, hp (horsepower), BTU/hr, cal/s\n"
                       "  - Temperature: degC (tempC), degF (tempF), K (tempK), degR (tempR)\n"
                       "  - Wire / Cross Section: AWG, mm^2, std_mm2, std_awg, kcmil, MCM\n"
                       "  - Decibel & Ratios: dB, dB_power, dB_v, dBm, dBW, dBV, dBu, dBmV, dBuV\n"
                       "  - Pure Ratios & Multipliers: times, ratio, x, percent (%), ppm, ppb\n"
                       "  - Electricity & Magnetism: A, V, ohm, F, H, C, Wb, T, G (gauss)\n"
                       "  - SI Prefixes (can prefix units): Y, Z, E, P, T, G, M, k, h, da, d, c, m, u, n, p, f, a, z, y\n"
                       "Note: 'h'=hectare in GNU Units; use 'hr' for hours";
        }
        else if (target == "len") {
            help_out = "=== len() Help ===\n"
                       "len(vector) : returns element count of vector/array";
        }
        else if (target == "sin" || target == "cos" || target == "tan" || target == "tg" || target == "ctan" || target == "cot" || target == "ctg" || target == "sec" || target == "csc" || target == "cosec" || target == "trig") {
            help_out = "=== Trigonometric Functions ===\n"
                       "sin(x), cos(x), tan(x)/tg(x), cot(x)/ctg(x), sec(x), csc(x)/cosec(x)\n"
                       "Respects Deg/Rad mode toggle.\n"
                       "Singularities: tan/sec (cos!=0), cot/csc (sin!=0)";
        }
        else if (target == "asin" || target == "arcsin" || target == "acos" || target == "arccos" || target == "atan" || target == "arctan" || target == "atg" || target == "arctg" || target == "acot" || target == "arccot" || target == "actg" || target == "arcctg" || target == "asec" || target == "arcsec" || target == "acsc" || target == "arccsc" || target == "acosec" || target == "arccosec" || target == "arctrig") {
            help_out = "=== Inverse Trigonometric Functions ===\n"
                       "asin(x), acos(x), atan(x)/arctg(x), acot(x)/arcctg(x), asec(x), acsc(x)\n"
                       "Returns angle in current Deg/Rad mode.\n"
                       "Domains: asin/acos [-1, 1], asec/acsc |x| >= 1";
        }
        else if (target == "sinh" || target == "sh" || target == "cosh" || target == "ch" || target == "tanh" || target == "th" || target == "coth" || target == "cth" || target == "ctgh" || target == "sech" || target == "sch" || target == "csch" || target == "cosech" || target == "hyperbolic") {
            help_out = "=== Hyperbolic Functions ===\n"
                       "sinh(x)/sh(x), cosh(x)/ch(x), tanh(x)/th(x), coth(x), sech(x), csch(x)\n"
                       "Real domain operations, independent of Deg/Rad mode.\n"
                       "Singularities: coth(x) and csch(x) require x != 0";
        }
        else if (target == "asinh" || target == "ahsin" || target == "arsh" || target == "acosh" || target == "ahcos" || target == "arch" || target == "atanh" || target == "ahtan" || target == "arth" || target == "acoth" || target == "ahcot" || target == "arcth" || target == "asech" || target == "ahsec" || target == "ahsech" || target == "acsch" || target == "ahcsc" || target == "ahcsch" || target == "archyp") {
            help_out = "=== Inverse Hyperbolic Functions ===\n"
                       "asinh(x)/arsh(x), acosh(x)/arch(x), atanh(x)/arth(x), acoth(x), asech(x), acsch(x)\n"
                       "Domains: acosh (x >= 1), atanh (-1 < x < 1), acoth (|x| > 1), asech (0 < x <= 1), acsch (x != 0)";
        }
        else if (target == "errors" || target == "domain") {
            help_out = "=== Domain Errors Reference ===\n"
                       "asin/acos : -1 <= x <= 1\n"
                       "asec/acsc : |x| >= 1\n"
                       "tan/sec   : undefined when cos(x) = 0\n"
                       "cot/csc   : undefined when sin(x) = 0\n"
                       "acosh     : x >= 1\n"
                       "atanh     : -1 < x < 1\n"
                       "acoth     : |x| > 1\n"
                       "asech     : 0 < x <= 1\n"
                       "coth/csch/acsch: x != 0\n"
                       "sqrt      : x >= 0\n"
                       "ln/log    : x > 0\n"
                       "fact      : n >= 0";
        }
        else if (target == "deg2rad" || target == "d2r" || target == "rad2deg" || target == "r2d") {
            help_out = "deg2rad(deg) / d2r(deg) : Convert degrees to radians\n"
                       "rad2deg(rad) / r2d(rad) : Convert radians to degrees";
        }
        else if (target == "sqrt" || target == "cbrt") {
            help_out = target + "(x) : Square root (x>=0) / Cube root";
        }
        else if (target == "ln" || target == "log" || target == "log2" || target == "logb") {
            help_out = "ln(x)    : Natural log (base e)\n"
                       "log(x)   : Base-10 log\n"
                       "log2(x)  : Base-2 log\n"
                       "logb(x,b): Custom base-b log";
        }
        else if (target == "abs" || target == "sgn" || target == "trunc" || target == "floor" || target == "ceil" || target == "round") {
            help_out = "abs(x)  : Absolute value |x|\n"
                       "sgn(x)  : Sign of x (-1, 0, 1)\n"
                       "floor(x): Round down to integer\n"
                       "ceil(x) : Round up to integer\n"
                       "round(x): Round to nearest integer\n"
                       "trunc(x): Integer part towards zero";
        }
        else if (target == "mod") {
            help_out = "mod(x, y) : Floating-point remainder (x % y)";
        }
        else if (target == "mean" || target == "median" || target == "mode" || target == "var" || target == "std") {
            help_out = target + "(a, b, c, ...) or " + target + "(vector) : Statistical calculation\n"
                       "Calculates " + target + " of values or 1D array";
        }
        else if (target == "dot") {
            help_out = "dot(v1, v2) or dot(a, b) : Dot product of vectors/scalars";
        }
        else if (target == "fact" || target == "C" || target == "Cnk" || target == "P" || target == "gcd" || target == "lcm" || target == "fib") {
            help_out = "fact(n) / n!      : Factorial n! (0 <= n <= 170)\n"
                       "C(n, k) / Cnk(n,k): Combinations nCr\n"
                       "P(n, k)           : Permutations nPr\n"
                       "gcd(a, b)         : Greatest Common Divisor\n"
                       "lcm(a, b)         : Least Common Multiple\n"
                       "fib(n)            : n-th Fibonacci number (0 <= n <= 1476)";
        }
        else if (target == "if" || target == "iff") {
            help_out = "if(cond, expr_true, expr_false) : Inline conditional\n"
                       "iff(c1, v1, c2, v2, ..., def)   : Multi-branch conditional";
        }
        else if (target == "sum" || target == "prod" || target == "min" || target == "max") {
            help_out = target + "(i, start, end, expr) : Inline loop over i = start..end\n" +
                       target + "(vector) or " + target + "(a, b, ...) : Aggregate over values";
        }
        else if (target == "rUni" || target == "rNor") {
            help_out = "rUni(min, max): Random float in range\n"
                       "rNor(mean, std): Normal random generator";
        }
        else {
            help_out = "Help: " + target + "\nGeneral math function/variable";
        }
        return true;
    }
    return false;
}
