package com.cardputer.cardulator

import android.text.Editable
import android.text.Spannable
import android.text.style.ForegroundColorSpan

internal data class SyntaxColors(
    val number: Int, val function: Int, val constant: Int,
    val keyword: Int, val operation: Int, val error: Int,
    val string: Int, val comment: Int, val brackets: IntArray
) {
    companion object {
        fun capture() = SyntaxColors(
            ThemePalette.purple, ThemePalette.aqua, ThemePalette.yellow,
            ThemePalette.red, ThemePalette.orange, ThemePalette.red,
            ThemePalette.green, ThemePalette.fgDim, ThemePalette.rainbowColors.toIntArray()
        )
    }
}

internal class CalculatorSyntaxHighlighter {
    private val highlightingEditables = java.util.Collections.newSetFromMap(java.util.IdentityHashMap<Editable, Boolean>())
    fun isApplying(editable: Editable): Boolean = highlightingEditables.contains(editable)

    private companion object {
        // Pre-compiled regex patterns for zero-allocation syntax highlighting
        private val NUMBER_REGEX = Regex("(?<![A-Za-z0-9_])(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?[a-zA-Z]*(?![A-Za-z0-9_])")
        private val KEYWORD_REGEX = Regex("\\b(for|while|if|iff|else|return|const|fn|def|function|and|or|not|xor)\\b")
        private val FUNC_REGEX = Regex("\\b(sin|cos|tan|tg|cot|ctg|ctan|sec|csc|cosec|asin|arcsin|acos|arccos|atan|arctan|atg|arctg|atan2|acot|arccot|actg|arcctg|actan|arcctan|asec|arcsec|acsc|arccsc|acosec|arccosec|sinh|sh|cosh|ch|tanh|th|coth|cth|ctgh|ctanh|sech|sch|csch|cosech|asinh|ahsin|arcsinh|arsinh|arsh|acosh|ahcos|arccosh|arcosh|arch|atanh|ahtan|arctanh|artanh|arth|artg|arctgh|acoth|ahcot|arccoth|arcoth|arcth|ahcoth|asech|ahsec|arcsech|arsech|ahsech|acsch|ahcsc|arccsch|arcsch|acosech|arccosech|ahcsch|deg2rad|d2r|rad2deg|r2d|sqrt|ln|log|log10|log2|logb|exp|cbrt|abs|ceil|floor|round|trunc|sgn|mod|mean|median|mode|var|std|sum|prod|min|max|dot|C|Cnk|P|rUni|rNor|fact|gcd|lcm|fib|len|print|run|plot|conv|help)\\b")
        private val CONST_REGEX = Regex("\\b(pi|phi|e\\d+)\\b|(?<![a-zA-Z0-9_])e(?![a-zA-Z0-9_])")
        private val OP_REGEX = Regex("[+\\-*/^=><!%]")
        private val SYNTAX_REGEX = Regex("(?:${NUMBER_REGEX.pattern})|(?:${KEYWORD_REGEX.pattern})|(?:${FUNC_REGEX.pattern})|(?:${CONST_REGEX.pattern})|(?:${OP_REGEX.pattern})")
    }

    // --- Syntax Highlighting & Bracket Matching ---
    // This method only touches the supplied text and palette, so detached output
    // can be prepared on a worker while editor highlighting stays on the UI thread.
    fun highlight(ssb: Spannable, startOffset: Int, text: String, palette: SyntaxColors = SyntaxColors.capture()) {
        val numColor = palette.number
        val funcColor = palette.function
        val constColor = palette.constant
        val keywordColor = palette.keyword
        val opColor = palette.operation
        val errColor = palette.error
        val masked = text.toCharArray()
        var quote: Char? = null
        var escaped = false
        var stringStart = -1
        var commentStart = -1
        for (i in text.indices) {
            val c = text[i]
            if (commentStart >= 0) {
                if (c == '\n' || c == '\r') {
                    ssb.setSpan(ForegroundColorSpan(palette.comment), startOffset + commentStart,
                        startOffset + i, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                    commentStart = -1
                } else masked[i] = ' '
                continue
            }
            if (quote == null && (c == '#' || (c == '/' && text.getOrNull(i + 1) == '/'))) {
                commentStart = i
                masked[i] = ' '
                continue
            }
            if (quote == null && (c == '"' || c == '\'')) {
                quote = c
                stringStart = i
            } else if (quote != null) {
                if (escaped) escaped = false
                else if (c == '\\') escaped = true
                else if (c == quote) {
                    ssb.setSpan(ForegroundColorSpan(palette.string), startOffset + stringStart, startOffset + i + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                    quote = null
                }
            }
            if (quote != null || stringStart >= 0) {
                masked[i] = ' '
                if (quote == null) stringStart = -1
            }
        }
        if (commentStart >= 0) ssb.setSpan(ForegroundColorSpan(palette.comment),
            startOffset + commentStart, startOffset + text.length, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
        if (quote != null) ssb.setSpan(ForegroundColorSpan(palette.string), startOffset + stringStart, startOffset + text.length, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
        val code = String(masked)

        for (match in SYNTAX_REGEX.findAll(code)) {
            val color = when {
                NUMBER_REGEX.matches(match.value) -> numColor
                KEYWORD_REGEX.matches(match.value) -> keywordColor
                FUNC_REGEX.matches(match.value) -> funcColor
                CONST_REGEX.matches(match.value) -> constColor
                else -> opColor
            }
            ssb.setSpan(ForegroundColorSpan(color), startOffset + match.range.first,
                startOffset + match.range.last + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
        }

        // 6. Rainbow Bracket Highlighting & Matching
        val colors = palette.brackets
        val numColors = colors.size
        var stackSize = 0
        val maxStack = 64
        val charStack = CharArray(maxStack)
        val posStack = IntArray(maxStack)

        for (i in text.indices) {
            val c = code[i]
            val absPos = startOffset + i
            if (c == '(' || c == '[' || c == '{') {
                val color = colors[stackSize % numColors]
                ssb.setSpan(ForegroundColorSpan(color), absPos, absPos + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                if (stackSize < maxStack) {
                    charStack[stackSize] = c
                    posStack[stackSize] = absPos
                    stackSize++
                }
            } else if (c == ')' || c == ']' || c == '}') {
                if (stackSize > 0) {
                    val topChar = charStack[stackSize - 1]
                    stackSize--
                    val matches = (topChar == '(' && c == ')') ||
                                  (topChar == '[' && c == ']') ||
                                  (topChar == '{' && c == '}')
                    if (matches) {
                        val color = colors[stackSize % numColors]
                        ssb.setSpan(ForegroundColorSpan(color), absPos, absPos + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                    } else {
                        ssb.setSpan(ForegroundColorSpan(errColor), absPos, absPos + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                    }
                } else {
                    ssb.setSpan(ForegroundColorSpan(errColor), absPos, absPos + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                }
            }
        }

        while (stackSize > 0) {
            stackSize--
            val unclosedPos = posStack[stackSize]
            ssb.setSpan(ForegroundColorSpan(errColor), unclosedPos, unclosedPos + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
        }
    }

    fun apply(editable: Editable) {
        if (!highlightingEditables.add(editable)) return
        try {
            val text = editable.toString()
            val highlighted = android.text.SpannableStringBuilder(text)
            highlight(highlighted, 0, text)
            data class SpanKey(val start: Int, val end: Int, val color: Int)
            val existing = mutableMapOf<SpanKey, ArrayDeque<ForegroundColorSpan>>()
            for (span in editable.getSpans(0, editable.length, ForegroundColorSpan::class.java)) {
                val key = SpanKey(editable.getSpanStart(span), editable.getSpanEnd(span), span.foregroundColor)
                existing.getOrPut(key) { ArrayDeque() }.addLast(span)
            }
            for (span in highlighted.getSpans(0, highlighted.length, ForegroundColorSpan::class.java)) {
                val key = SpanKey(highlighted.getSpanStart(span), highlighted.getSpanEnd(span), span.foregroundColor)
                val reusable = existing[key]
                if (reusable != null && reusable.isNotEmpty()) reusable.removeFirst()
                else editable.setSpan(ForegroundColorSpan(key.color), key.start, key.end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
            }
            for (spans in existing.values) for (span in spans) editable.removeSpan(span)
        } finally {
            highlightingEditables.remove(editable)
        }
    }
}
