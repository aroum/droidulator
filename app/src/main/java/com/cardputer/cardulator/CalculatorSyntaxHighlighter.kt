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

    /** Stored on its own Editable, excluded when Android copies the text. */
    private class TokenCache : android.text.NoCopySpan {
        val tokens = IncrementalSyntaxTokens()
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

    private fun spans(text: String, palette: SyntaxColors, cache: IncrementalSyntaxTokens? = null): List<SpanKey> {
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

        val tokens = cache?.update(code) ?: IncrementalSyntaxTokens.scan(code)
        for (token in tokens) {
            val color = when (token.kind) {
                IncrementalSyntaxTokens.Kind.NUMBER -> numColor
                IncrementalSyntaxTokens.Kind.KEYWORD -> keywordColor
                IncrementalSyntaxTokens.Kind.FUNCTION -> funcColor
                IncrementalSyntaxTokens.Kind.CONSTANT -> constColor
                IncrementalSyntaxTokens.Kind.OPERATION -> opColor
            }
            spans.add(SpanKey(token.start, token.end, color))
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
            val cache = editable.getSpans(0, editable.length, TokenCache::class.java).firstOrNull()
                ?: TokenCache().also { editable.setSpan(it, 0, 0, Spannable.SPAN_MARK_MARK) }
            // Masking strings/comments and validating bracket balance are cheap full scans.
            // Only changed masked lines run the expensive token regex again. A quote or
            // comment edit can therefore invalidate every affected downstream line safely.
            val planned = performanceTrace("Droidulator.highlightPlan") {
                spans(text, SyntaxColors.capture(), cache.tokens)
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
