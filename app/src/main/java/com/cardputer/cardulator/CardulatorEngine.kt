package com.cardputer.cardulator

data class NativePlotLine(val x: DoubleArray, val y: DoubleArray, val color: Int, val linestyle: String)

data class NativeValue(val name: String, val scalar: Double, val elements: DoubleArray?) {
    fun expressionValue(): String = elements?.joinToString(", ", "[", "]") ?: scalar.toString()
}

class CardulatorEngine : AutoCloseable {
    companion object {
        init { System.loadLibrary("cardulator_jni") }

        @JvmStatic external fun nativeInitUnits(unitsDataPath: String)
    }

    private var nativeHandle = nativeCreate()
    private external fun nativeCreate(): Long
    private external fun nativeDestroy(handle: Long)

    override fun close() {
        val handle = nativeHandle
        nativeHandle = 0
        if (handle != 0L) nativeDestroy(handle)
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
    fun evaluateEntry(input: String): ReplDisplayItem {
        val evaluation = nativeEvaluateDetailed(input)
        val isError = evaluation[0] == "error"
        val hasNum = !isError && nativeHasLastNumeric()
        return ReplDisplayItem(
            input = input, rawOutput = evaluation[1], isError = isError, hasNum = hasNum,
            arrayValues = if (!isError && !hasNum) nativeLastArray() else null,
            numVal = if (hasNum) nativeGetLastNumericVal() else 0.0,
            prefix = if (hasNum) nativeGetLastNumericPrefix() else "",
            suffix = if (hasNum) nativeGetLastNumericSuffix() else ""
        )
    }

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
    external fun nativeValues(constants: Boolean): Array<NativeValue>
    external fun nativeFormulas(): Array<String>
    external fun nativeGetHelp(topic: String): String
    external fun nativeSetDegRad(isDeg: Boolean)
    external fun nativeGetDegRad(): Boolean
    external fun nativeSetFormatSettings(mode: Int, precision: Int, groupDigits: Boolean)
    external fun nativeDeleteVariable(name: String): Boolean
    external fun nativeDeleteConstant(name: String): Boolean
    external fun nativeDeleteFormula(name: String): Boolean
    external fun nativeGetPlotLines(): Array<NativePlotLine>
    external fun nativeGetPlotLimits(): DoubleArray
    external fun nativeRestorePlot(lines: Array<NativePlotLine>, limits: DoubleArray)
    external fun nativeLastArray(): DoubleArray?
    external fun nativeFormatNumber(value: Double): String
    external fun nativeHasLastNumeric(): Boolean
    external fun nativeGetLastNumericVal(): Double
    external fun nativeGetLastNumericPrefix(): String
    external fun nativeGetLastNumericSuffix(): String
    external fun nativeGetConvUnits(fromUnit: String, prefix: String): String
}
