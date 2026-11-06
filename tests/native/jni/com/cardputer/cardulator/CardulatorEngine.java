package com.cardputer.cardulator;
public class CardulatorEngine {

  native void nativeInit(); native void nativeInitUnits(String path); native String nativeEvaluate(String s);
  native double nativeGetLastNumericVal();
  native String nativeGetVariables(); native NativePlotLine[] nativeGetPlotLines(); native double[] nativeGetPlotLimits();
  native String[][] nativeGetScriptFunctions(); native void nativeRestoreScriptFunction(String[] fields);
  native void nativeRestoreScalar(String name,double value,boolean constant); native void nativeRestoreArray(String name,double[] vals);
  public static void main(String[] args) {
    System.load(args[0]);
    CardulatorEngine e = new CardulatorEngine(); e.nativeInit(); e.nativeInitUnits(args[1]);
    if (!e.nativeEvaluate("").isEmpty()) throw new AssertionError("empty string");
    String x=e.nativeEvaluate("print(\"Привет 😀\")"); if (!x.contains("Привет 😀")) throw new AssertionError(x);
    String withNul = e.nativeEvaluate("print(\"a\u0000b\")");
    if (!withNul.contains("a\u0000b")) throw new AssertionError("NUL in output");
    for (int i=1;i<=130;i++) e.nativeEvaluate(Integer.toString(i));
    String all=e.nativeGetVariables(); if (!all.contains("e3 = 3") || !all.contains("e130 = 130") || all.contains("e2 = 2\n")) throw new AssertionError(all);
    if (!e.nativeEvaluate("e3=1").startsWith("ERROR")) throw new AssertionError("eN writable");
    e.nativeRestoreScalar("x",Double.POSITIVE_INFINITY,false); if (!e.nativeGetVariables().contains("x = inf")) throw new AssertionError("infinity");
    e.nativeRestoreArray("arr",new double[]{1.0,Double.NaN}); if (!e.nativeGetVariables().contains("arr = [1, nan]")) throw new AssertionError("array");
    e.nativeRestoreScriptFunction(new String[]{"abc","1","x","return x*2"}); if(e.nativeGetScriptFunctions().length != 1) throw new AssertionError("function");
    String zero=e.nativeEvaluate("conv(0, \"rad\")");
    if (zero.startsWith("ERROR") || !zero.contains("0")) throw new AssertionError(zero);
    String bad=e.nativeEvaluate("conv(unknown_variable, \"m\", \"cm\")");
    if (!bad.startsWith("ERROR")) throw new AssertionError(bad);
    String numeric=e.nativeEvaluate("conv(104, \"pf\")");
    double numericValue=e.nativeGetLastNumericVal();
    String marking=e.nativeEvaluate("conv(\"104\", \"pf\")");
    double markingValue=e.nativeGetLastNumericVal();
    if (numeric.startsWith("ERROR") || marking.startsWith("ERROR") || Math.abs(numericValue - 104e-12) > 1e-18 || Math.abs(markingValue - 100000) > 1e-6) throw new AssertionError(numeric + " / " + marking);
    String mega=e.nativeEvaluate("conv(1, \"Mm\", \"m\")");
    if (mega.startsWith("ERROR") || Math.abs(e.nativeGetLastNumericVal() - 1e6) > 1e-6) throw new AssertionError(mega);
    String p=e.nativeEvaluate("plot([0,1], [1234567.89,2])"); if (p.startsWith("ERROR")) throw new AssertionError(p);
    NativePlotLine[] lines=e.nativeGetPlotLines(); if(lines.length != 1 || lines[0].y[0] != 1234567.89 || e.nativeGetPlotLimits().length != 5) throw new AssertionError("plot");
    System.out.println("JNI tests passed");
  }
}
