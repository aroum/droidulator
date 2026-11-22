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

    private data class SpanKey(val start: Int, val end: Int, val color: Int)

    private companion object {
        // Each alternative owns one capture group, so classification needs no second regex match.
        private val NUMBER_REGEX = Regex("(?<![A-Za-z0-9_])(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?[a-zA-Z]*(?![A-Za-z0-9_])")
        private val KEYWORD_REGEX = Regex("\\b(?:for|while|if|iff|else|return|const|fn|def|function|and|or|not|xor)\\b")
        private val FUNC_REGEX = Regex("\\b(?:sin|cos|tan|tg|cot|ctg|ctan|sec|csc|cosec|asin|arcsin|acos|arccos|atan|arctan|atg|arctg|atan2|acot|arccot|actg|arcctg|actan|arcctan|asec|arcsec|acsc|arccsc|acosec|arccosec|sinh|sh|cosh|ch|tanh|th|coth|cth|ctgh|ctanh|sech|sch|csch|cosech|asinh|ahsin|arcsinh|arsinh|arsh|acosh|ahcos|arccosh|arcosh|arch|atanh|ahtan|arctanh|artanh|arth|artg|arctgh|acoth|ahcot|arccoth|arcoth|arcth|ahcoth|asech|ahsec|arcsech|arsech|ahsech|acsch|ahcsc|arccsch|arcsch|acosech|arccosech|ahcsch|deg2rad|d2r|rad2deg|r2d|sqrt|ln|log|log10|log2|logb|exp|cbrt|abs|ceil|floor|round|trunc|sgn|mod|mean|median|mode|var|std|sum|prod|min|max|dot|C|Cnk|P|rUni|rNor|fact|gcd|lcm|fib|len|print|run|plot|conv|help)\\b")
        private val CONST_REGEX = Regex("\\b(?:pi|phi|e\\d+)\\b|(?<![a-zA-Z0-9_])e(?![a-zA-Z0-9_])")
        private val OP_REGEX = Regex("[+\\-*/^=><!%]")
        private val SYNTAX_REGEX = Regex("(${NUMBER_REGEX.pattern})|(${KEYWORD_REGEX.pattern})|(${FUNC_REGEX.pattern})|(${CONST_REGEX.pattern})|(${OP_REGEX.pattern})")
    }

    // --- Syntax Highlighting & Bracket Matching ---
    // This method only touches the supplied text and palette, so detached output
    // can be prepared on a worker while editor highlighting stays on the UI thread.
    fun highlight(ssb: Spannable, startOffset: Int, text: String, palette: SyntaxColors = SyntaxColors.capture()) {
        for (span in spans(text, palette)) {
            ssb.setSpan(ForegroundColorSpan(span.color), startOffset + span.start,
                startOffset + span.end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
        }
    }

    private fun spans(text: String, palette: SyntaxColors): List<SpanKey> {
        val spans = ArrayList<SpanKey>()
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
                    spans.add(SpanKey(commentStart, i, palette.comment))
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
                    spans.add(SpanKey(stringStart, i + 1, palette.string))
                    quote = null
                }
            }
            if (quote != null || stringStart >= 0) {
                masked[i] = ' '
                if (quote == null) stringStart = -1
            }
        }
        if (commentStart >= 0) spans.add(SpanKey(commentStart, text.length, palette.comment))
        if (quote != null) spans.add(SpanKey(stringStart, text.length, palette.string))
        val code = String(masked)

        for (match in SYNTAX_REGEX.findAll(code)) {
            val color = when {
                match.groups[1] != null -> numColor
                match.groups[2] != null -> keywordColor
                match.groups[3] != null -> funcColor
                match.groups[4] != null -> constColor
                else -> opColor
            }
            spans.add(SpanKey(match.range.first, match.range.last + 1, color))
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
            if (c == '(' || c == '[' || c == '{') {
                val color = colors[stackSize % numColors]
                spans.add(SpanKey(i, i + 1, color))
                if (stackSize < maxStack) {
                    charStack[stackSize] = c
                    posStack[stackSize] = i
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
                        spans.add(SpanKey(i, i + 1, color))
                    } else {
                        spans.add(SpanKey(i, i + 1, errColor))
                    }
                } else {
                    spans.add(SpanKey(i, i + 1, errColor))
                }
            }
        }

        while (stackSize > 0) {
            stackSize--
            val unclosedPos = posStack[stackSize]
            spans.add(SpanKey(unclosedPos, unclosedPos + 1, errColor))
        }
        return spans
    }

    fun apply(editable: Editable) {
        if (!highlightingEditables.add(editable)) return
        android.os.Trace.beginSection("Droidulator.highlight")
        try {
            val text = editable.toString()
            // Plan plain ranges; constructing a second spanned buffer for every edit
            // makes Android repeatedly maintain thousands of unrelated span indices.
            val planned = performanceTrace("Droidulator.highlightPlan") {
                spans(text, SyntaxColors.capture())
            }
            performanceTrace("Droidulator.highlightSync") {
                val existing = mutableMapOf<SpanKey, ArrayDeque<ForegroundColorSpan>>()
                for (span in editable.getSpans(0, editable.length, ForegroundColorSpan::class.java)) {
                    val key = SpanKey(editable.getSpanStart(span), editable.getSpanEnd(span), span.foregroundColor)
                    existing.getOrPut(key) { ArrayDeque() }.addLast(span)
                }
                for (key in planned) {
                    val reusable = existing[key]
                    if (reusable != null && reusable.isNotEmpty()) reusable.removeFirst()
                    else editable.setSpan(ForegroundColorSpan(key.color), key.start, key.end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                }
                for (spans in existing.values) for (span in spans) editable.removeSpan(span)
            }
        } finally {
            android.os.Trace.endSection()
            highlightingEditables.remove(editable)
        }
    }
}
