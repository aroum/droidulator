package com.cardputer.cardulator

import android.text.Spannable
import android.text.SpannableStringBuilder
import android.text.style.ForegroundColorSpan

internal class ReplOutputRenderer(private val highlighter: CalculatorSyntaxHighlighter) {
    data class Palette(val prompt: Int, val error: Int, val result: Int, val syntax: SyntaxColors) {
        companion object {
            fun capture() = Palette(ThemePalette.fgDim, ThemePalette.red, ThemePalette.green, SyntaxColors.capture())
        }
    }

    fun entry(input: String, result: String, isError: Boolean, palette: Palette = Palette.capture()): SpannableStringBuilder {
        val text = SpannableStringBuilder()
        appendColored(text, "> ", palette.prompt)
        val expressionStart = text.length
        text.append(input)
        highlighter.highlight(text, expressionStart, input, palette.syntax)
        text.append("\n")
        val prefix = if (isError || input.startsWith("print(") || input.startsWith("plot(")) "= " else ""
        appendColored(text, prefix + result + "\n", if (isError) palette.error else palette.result)
        return text
    }

    fun render(
        items: List<ReplDisplayItem>,
        welcome: String,
        showWelcome: Boolean,
        results: List<String>,
        palette: Palette = Palette.capture()
    ): SpannableStringBuilder {
        val text = SpannableStringBuilder()
        if (showWelcome) appendColored(text, welcome, palette.prompt)
        for ((index, item) in items.withIndex()) {
            if (item.isRawText) text.append(item.rawOutput)
            else text.append(entry(item.input, results[index], item.isError, palette))
        }
        return text
    }

    companion object {
        fun formatResults(items: List<ReplDisplayItem>, formatNumbers: (DoubleArray) -> Array<String>): List<String> =
            items.map { item ->
                when {
                    item.isRawText -> item.rawOutput
                    item.arrayValues != null -> item.rawOutput.substringBeforeLast("[") +
                        formatNumbers(item.arrayValues).joinToString(", ", "[", "]")
                    item.hasNum -> item.prefix + formatNumbers(doubleArrayOf(item.numVal)).single() + item.suffix
                    else -> item.rawOutput
                }
            }
    }

    private fun appendColored(text: SpannableStringBuilder, value: String, color: Int) {
        val start = text.length
        text.append(value)
        text.setSpan(ForegroundColorSpan(color), start, text.length, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
    }
}
