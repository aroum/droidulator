package com.cardputer.cardulator

data class NativePlotLine(val x: DoubleArray, val y: DoubleArray, val color: Int, val linestyle: String)

object CardulatorEngine {
    init {
        System.loadLibrary("cardulator_jni")
    }

    private var initialized = false

    @Synchronized
    fun initializeOnce(): Boolean {
        if (initialized) return false
        nativeInit()
        initialized = true
        return true
    }

    private external fun nativeInit()
    external fun nativeInitUnits(unitsDataPath: String)
    external fun nativeEvaluate(input: String): String
    external fun nativeEvaluateDetailed(input: String): Array<String>
    external fun nativeEvaluatePreview(input: String): String
    external fun nativeAutocomplete(input: String, caretPos: Int, outCursor: IntArray): String
    external fun nativeResetAutocomplete()
    external fun nativeRunScript(code: String): String
    external fun nativeRegisterScript(name: String, code: String)
    external fun nativeUnregisterScript(name: String)
    external fun nativeGetScriptFunctions(): Array<Array<String>>
    external fun nativeRestoreScriptFunction(fields: Array<String>)
    external fun nativeRestoreScalar(name: String, value: Double, isConstant: Boolean)
    external fun nativeRestoreArray(name: String, values: DoubleArray)
    external fun nativeClearHistory()
    external fun nativeGetVariables(): String
    external fun nativeGetFormulas(): String
    external fun nativeGetConstants(): String
    external fun nativeGetHelp(topic: String): String
    external fun nativeSetDegRad(isDeg: Boolean)
    external fun nativeGetDegRad(): Boolean
    external fun nativeSetFormatSettings(mode: Int, precision: Int, groupDigits: Boolean)
    external fun nativeDeleteVariable(name: String): Boolean
    external fun nativeDeleteConstant(name: String): Boolean
    external fun nativeDeleteFormula(name: String): Boolean
    external fun nativeGetPlotLines(): Array<NativePlotLine>
    external fun nativeGetPlotLimits(): DoubleArray
    external fun nativeFormatNumber(value: Double): String
    external fun nativeHasLastNumeric(): Boolean
    external fun nativeGetLastNumericVal(): Double
    external fun nativeGetLastNumericPrefix(): String
    external fun nativeGetLastNumericSuffix(): String
    external fun nativeGetConvUnits(fromUnit: String, prefix: String): String
}
