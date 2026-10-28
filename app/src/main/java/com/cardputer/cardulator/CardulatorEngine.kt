package com.cardputer.cardulator

object CardulatorEngine {
    init {
        try {
            System.loadLibrary("cardulator_jni")
        } catch (e: UnsatisfiedLinkError) {
            e.printStackTrace()
        }
    }

    external fun nativeInit()
    external fun nativeInitUnits(unitsDataPath: String)
    external fun nativeEvaluate(input: String): String
    external fun nativeEvaluatePreview(input: String): String
    external fun nativeAutocomplete(input: String, caretPos: Int, outCursor: IntArray): String
    external fun nativeRunScript(code: String): String
    external fun nativeRegisterScript(name: String, code: String)
    external fun nativeClearHistory()
    external fun nativeGetVariables(): String
    external fun nativeGetFormulas(): String
    external fun nativeGetConstants(): String
    external fun nativeGetHelp(topic: String): String
    external fun nativeSetDegRad(isDeg: Boolean)
    external fun nativeSetFormatSettings(mode: Int, precision: Int, groupDigits: Boolean)
    external fun nativeDeleteVariable(name: String): Boolean
    external fun nativeDeleteConstant(name: String): Boolean
    external fun nativeDeleteFormula(name: String): Boolean
    external fun nativeGetPlotLines(): String
    external fun nativeFormatNumber(value: Double): String
    external fun nativeHasLastNumeric(): Boolean
    external fun nativeGetLastNumericVal(): Double
    external fun nativeGetLastNumericPrefix(): String
    external fun nativeGetLastNumericSuffix(): String
    external fun nativeGetConvUnits(fromUnit: String, prefix: String): String
}
