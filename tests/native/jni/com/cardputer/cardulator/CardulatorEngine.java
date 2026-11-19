package com.cardputer.cardulator;
public class CardulatorEngine {

  private final long nativeHandle = nativeCreate();
  native long nativeCreate();
  native void nativeDestroy(long handle);
  native void nativeInit(); static native void nativeInitUnits(String path); native String nativeEvaluate(String s);
  native String[] nativeEvaluateDetailed(String s);
  native void nativeRegisterScript(String name, String code); native void nativeUnregisterScript(String name);
  native double nativeGetLastNumericVal();
  native NativeValue[] nativeValues(boolean constants);
  native NativeFormula[] nativeFormulas();
  native NativePlotLine[] nativeGetPlotLines(); native double[] nativeGetPlotLimits();
  native String[][] nativeGetScriptFunctions(); native void nativeRestoreScriptFunction(String[] fields);
  native void nativeRestorePlot(NativePlotLine[] lines, double[] limits);
  native void nativeRestoreScalar(String name,double value,boolean constant); native void nativeRestoreArray(String name,double[] vals);
  public static void main(String[] args) {
    System.load(args[0]);
    CardulatorEngine e = new CardulatorEngine(); e.nativeInit(); e.nativeInitUnits(args[1]);
    CardulatorEngine second = new CardulatorEngine();
    second.nativeInit();
    e.nativeEvaluate("session_value=42");
    second.nativeEvaluate("session_value=17");
    e.nativeEvaluate("session_value");
    second.nativeEvaluate("session_value");
    if (e.nativeGetLastNumericVal() != 42 || second.nativeGetLastNumericVal() != 17)
      throw new AssertionError("JNI sessions share state");
    second.nativeDestroy(second.nativeHandle);
    e.nativeInit();
    if (!e.nativeEvaluate("").isEmpty()) throw new AssertionError("empty string");
    String x=e.nativeEvaluate("print(\"Привет 😀\")"); if (!x.contains("Привет 😀")) throw new AssertionError(x);
    String withNul = e.nativeEvaluate("print(\"a\u0000b\")");
    if (!withNul.contains("a\u0000b")) throw new AssertionError("NUL in output");
    for (int i=1;i<=130;i++) e.nativeEvaluate(Integer.toString(i));
    NativeValue[] history = e.nativeValues(false);
    if (history.length != 128 || !history[0].name.equals("e3") || history[0].scalar != 3 ||
        !history[127].name.equals("e130") || history[127].scalar != 130)
      throw new AssertionError("typed history eviction or ordering");
    if (!e.nativeEvaluate("e3=1").startsWith("ERROR")) throw new AssertionError("eN writable");
    String[] printedError = e.nativeEvaluateDetailed("print(\"ERROR: x\")");
    if (!printedError[0].equals("ok") || !printedError[1].equals("ERROR: x")) throw new AssertionError("print status");
    String[] actualError = e.nativeEvaluateDetailed("sqrt(-1)");
    if (!actualError[0].equals("error")) throw new AssertionError("error status");
    for (String unit : new String[]{"rad", "rpm", "rem", "rankine"}) {
      String result = e.nativeEvaluate("conv(0, \"" + unit + "\")");
      if (result.startsWith("ERROR") || !result.contains("0")) throw new AssertionError(unit + ": " + result);
    }
    e.nativeRegisterScript("named_test", "print(42)");
    if (e.nativeEvaluate("run(\"named_test\")").startsWith("ERROR")) throw new AssertionError("script registration");
    e.nativeUnregisterScript("named_test");
    if (!e.nativeEvaluate("run(\"named_test\")").startsWith("ERROR")) throw new AssertionError("script deletion");
    for (String[] tc : new String[][]{{"3!+4!", "30"}, {"(2+1)!*2!", "12"}, {"2 xor 3", "0"}, {"0 xor 3", "1"}}) {
      String output = e.nativeEvaluate(tc[0]);
      if (output.startsWith("ERROR") || e.nativeGetLastNumericVal() != Double.parseDouble(tc[1])) throw new AssertionError(tc[0] + ": " + output);
    }
    e.nativeRestoreScalar("x",Double.POSITIVE_INFINITY,false);
    e.nativeRestoreArray("arr",new double[]{1.0,Double.NaN});
    NativeValue[] snapshot = e.nativeValues(false);
    if (java.util.Arrays.stream(snapshot).noneMatch(v -> v.name.equals("x") && v.scalar == Double.POSITIVE_INFINITY && v.elements == null))
      throw new AssertionError("typed scalar");
    if (java.util.Arrays.stream(snapshot).noneMatch(v -> v.name.equals("arr") && v.elements.length == 2 && Double.isNaN(v.elements[1])))
      throw new AssertionError("typed array");
    e.nativeRestoreScalar("jni_const", 42, true);
    if (java.util.Arrays.stream(e.nativeValues(true)).noneMatch(v -> v.name.equals("jni_const") && v.scalar == 42 && v.elements == null))
      throw new AssertionError("typed constant");
    e.nativeEvaluate("jni_formula(x)=x^2");
    if (java.util.Arrays.stream(e.nativeFormulas()).noneMatch(f -> f.name.equals("jni_formula") && f.parameters.length == 1 && f.parameters[0].equals("x") && f.body.equals("x^2")))
      throw new AssertionError("formula snapshot");
    e.nativeRestoreScriptFunction(new String[]{"abc","1","x","return x*2"}); if(e.nativeGetScriptFunctions().length != 1) throw new AssertionError("function");
    String zero=e.nativeEvaluate("conv(0, \"rad\")");
    if (zero.startsWith("ERROR") || !zero.contains("0")) throw new AssertionError(zero);
    String bad=e.nativeEvaluate("conv(unknown_variable, \"m\", \"cm\")");
    if (!bad.startsWith("ERROR")) throw new AssertionError(bad);
    String nestedDefinition = e.nativeEvaluate("fn nested_test(x) { if(x>0) { return x } else { return -1 } }; 7");
    if (nestedDefinition.startsWith("ERROR") || e.nativeGetLastNumericVal() != 7) throw new AssertionError(nestedDefinition);
    if (e.nativeEvaluate("nested_test(2)").startsWith("ERROR") || e.nativeGetLastNumericVal() != 2) throw new AssertionError("nested function");
    e.nativeEvaluate("fn recursion_test(x) { return recursion_test(x) }");
    if (!e.nativeEvaluate("recursion_test(1)").contains("recursion limit")) throw new AssertionError("recursion limit");
    String printedFunction = e.nativeEvaluate("fn print_test(x) { print(x); return x*2 }; print_test(4)");
    if (!printedFunction.contains("4\n") || e.nativeGetLastNumericVal() != 8) throw new AssertionError(printedFunction);
    String numeric=e.nativeEvaluate("conv(104, \"pf\")");
    double numericValue=e.nativeGetLastNumericVal();
    String marking=e.nativeEvaluate("conv(\"104\", \"pf\")");
    double markingValue=e.nativeGetLastNumericVal();
    if (numeric.startsWith("ERROR") || marking.startsWith("ERROR") || Math.abs(numericValue - 104e-12) > 1e-18 || Math.abs(markingValue - 100000) > 1e-6) throw new AssertionError(numeric + " / " + marking);
    String mega=e.nativeEvaluate("conv(1, \"Mm\", \"m\")");
    if (mega.startsWith("ERROR") || Math.abs(e.nativeGetLastNumericVal() - 1e6) > 1e-6) throw new AssertionError(mega);
    String p=e.nativeEvaluate("plot([0,1], [1234567.89,2])"); if (p.startsWith("ERROR")) throw new AssertionError(p);
    NativePlotLine[] lines=e.nativeGetPlotLines(); if(lines.length != 1 || lines[0].y[0] != 1234567.89 || e.nativeGetPlotLimits().length != 7) throw new AssertionError("plot");
    double[] plotLimits = e.nativeGetPlotLimits();
    e.nativeEvaluate("plot.close()");
    e.nativeRestorePlot(lines, plotLimits);
    if (e.nativeGetPlotLines().length != 1 || e.nativeGetPlotLines()[0].y[0] != 1234567.89) throw new AssertionError("restore plot");
    System.out.println("JNI tests passed");
  }
}


final class NativeValue {
  final String name;
  final double scalar;
  final double[] elements;

  NativeValue(String name, double scalar, double[] elements) {
    this.name = name;
    this.scalar = scalar;
    this.elements = elements;
  }
}

final class NativeFormula {
  final String name;
  final String[] parameters;
  final String body;

  NativeFormula(String name, String[] parameters, String body) {
    this.name = name;
    this.parameters = parameters;
    this.body = body;
  }
}
