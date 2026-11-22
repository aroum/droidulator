package com.cardputer.cardulator

/** Finds the active conv argument without depending on an editor or Activity. */
internal object ConversionCompletion {
    fun contextBeforeCaret(text: String, caret: Int): Pair<Int, List<String>>? {
        if (caret !in 0..text.length) return null
        val groups = mutableListOf<Pair<Char, Int>>()
        var quote: Char? = null
        var escaped = false
        for (i in 0 until caret) {
            val c = text[i]
            if (quote != null) {
                if (escaped) escaped = false
                else if (c == '\\') escaped = true
                else if (c == quote) quote = null
            } else when (c) {
                '\'', '"' -> quote = c
                '(', '[', '{' -> groups.add(c to i)
                ')', ']', '}' -> {
                    val expected = when (c) { ')' -> '('; ']' -> '['; else -> '{' }
                    if (groups.lastOrNull()?.first != expected) return null
                    groups.removeAt(groups.lastIndex)
                }
            }
        }
        val (group, open) = groups.lastOrNull() ?: return null
        if (group != '(') return null
        var nameEnd = open
        while (nameEnd > 0 && text[nameEnd - 1].isWhitespace()) nameEnd--
        val nameStart = nameEnd - 4
        if (nameStart < 0 || text.substring(nameStart, nameEnd) != "conv") return null
        if (nameStart > 0 && (text[nameStart - 1].isLetterOrDigit() || text[nameStart - 1] == '_')) return null

        val parts = mutableListOf<String>()
        var start = open + 1
        var depth = 0
        quote = null
        escaped = false
        for (i in start until caret) {
            val c = text[i]
            if (quote != null) {
                if (escaped) escaped = false
                else if (c == '\\') escaped = true
                else if (c == quote) quote = null
            } else when (c) {
                '\'', '"' -> quote = c
                '(', '[', '{' -> depth++
                ')', ']', '}' -> depth--
                ',' -> if (depth == 0) {
                    parts.add(text.substring(start, i))
                    start = i + 1
                }
            }
        }
        parts.add(text.substring(start, caret))
        return start to parts
    }
}
