package com.cardputer.cardulator

/** Owns completion matching and cycling; the view only applies the returned edit. */
internal class AutocompleteController(
    private val units: (String, String) -> List<String>,
    private val scriptNames: () -> List<String>,
    private val general: (String, Int) -> Edit?
) {
    data class Edit(val text: String, val caret: Int)
    private var convTabOrigPrefix: String? = null
    private var convTabSplitPos = -1
    private var convTabMatchIdx = 0
    private var convTabCachedMatches: List<String>? = null
    private var lastEdit: Edit? = null

    fun reset() {
        convTabOrigPrefix = null
        convTabSplitPos = -1
        convTabMatchIdx = 0
        convTabCachedMatches = null
        lastEdit = null
    }

    fun complete(text: String, caret: Int): Edit? {
        if (caret !in 0..text.length) return null
        if (lastEdit != Edit(text, caret)) reset()
        return compute(text, caret).also { lastEdit = it }
    }

    private fun compute(fullText: String, caret: Int): Edit? {
        // Detect conv( context: suggest units/resistor codes inside conv(...)
        val convContext = ConversionCompletion.contextBeforeCaret(fullText, caret)
        if (convContext != null) {
            val (splitPos, parts) = convContext

            val isContinuation = (convTabOrigPrefix != null && convTabSplitPos == splitPos && convTabCachedMatches != null)
            if (isContinuation) {
                val cached = convTabCachedMatches!!
                if (cached.isNotEmpty()) {
                    convTabMatchIdx++
                    val match = cached[convTabMatchIdx % cached.size]

                    return conversionEdit(fullText, caret, splitPos, match)
                }
            }

            val lastPartRaw = parts.last().trim()
            val lastPart = lastPartRaw.trim('"', '\'')

            // Detect source unit (fromUnit) if present in preceding arguments
            val fromUnit: String? = when {
                parts.size >= 3 -> {
                    val prevArg = parts[parts.size - 2].trim().trim('"', '\'')
                    if (prevArg.isNotEmpty()) prevArg else null
                }
                parts.size == 2 -> {
                    val firstArg = parts[0].trim().trim('"', '\'')
                    // Check if first arg contains a number followed by a unit (e.g. "10 mm", "10mm", "100 ohm")
                    val m = Regex("""^[\d.\s\+\-\*\/\^eE]+\s*([a-zA-Z][a-zA-Z\^\d_]*)$""").find(firstArg)
                    m?.groupValues?.get(1)?.trim()
                }
                else -> null
            }

            convTabOrigPrefix = lastPart
            convTabSplitPos = splitPos
            convTabMatchIdx = 0

            val matches = units(fromUnit.orEmpty(), lastPart)

            if (matches.isNotEmpty()) {
                convTabCachedMatches = matches
                val match = matches[0]

                return conversionEdit(fullText, caret, splitPos, match)
            } else {
                convTabOrigPrefix = null
                convTabSplitPos = -1
                convTabMatchIdx = 0
                convTabCachedMatches = null
            }
        } else {
            convTabOrigPrefix = null
            convTabSplitPos = -1
            convTabMatchIdx = 0
            convTabCachedMatches = null
        }

        // Detect plot( context: suggest plot parameters/functions inside plot(...)
        val plotMatch = Regex("""plot\(([^)]*)$""").find(fullText.substring(0, caret))
        if (plotMatch != null) {
            val argsContent = plotMatch.groupValues[1]
            val parts = argsContent.split(",")
            val lastPart = parts.last().trim().trim('"', '\'')
            val plotCandidates = listOf(
                "sin(x)", "cos(x)", "tan(x)", "exp(x)", "log(x)", "sqrt(x)",
                "x^2", "x^3", "abs(x)", "xlim", "ylim"
            )
            val match = if (lastPart.isEmpty()) {
                "sin(x)"
            } else {
                plotCandidates.firstOrNull { it.startsWith(lastPart, ignoreCase = true) && !it.equals(lastPart, ignoreCase = true) }
                    ?: plotCandidates.firstOrNull { it.startsWith(lastPart, ignoreCase = true) }
            }
            if (match != null) {
                val lastCommaPos = fullText.lastIndexOf(',', caret - 1)
                val lastParenPos = fullText.lastIndexOf('(', caret - 1)
                val splitPos = if (lastCommaPos > lastParenPos) lastCommaPos + 1 else lastParenPos + 1
                val prefix = fullText.substring(0, splitPos)
                val suffix = fullText.substring(caret)
                val newText = prefix + match + suffix
                return Edit(newText, prefix.length + match.length)
            }
        }

        // Detect run( context: complete script name from userScripts
        val runMatch = Regex("""run\("?([\w]*)"?\)?$""").find(fullText.substring(0, caret))
        if (runMatch != null) {
            val partial = runMatch.groupValues[1]
            val match = scriptNames().firstOrNull { it.startsWith(partial, ignoreCase = true) }
            if (match != null) {
                val runCallStart = fullText.lastIndexOf("run(", caret - 1)
                if (runCallStart != -1) {
                    val baseText = fullText.substring(0, runCallStart)
                    val replacement = "run(\"$match\")"
                    val newText = baseText + replacement + fullText.substring(caret)
                    return Edit(newText, baseText.length + replacement.length)
                }
            }
        }

        return general(fullText, caret)?.takeIf { it.text != fullText }
    }

    private fun conversionEdit(text: String, caret: Int, splitPos: Int, match: String): Edit {
        val prefix = text.substring(0, splitPos).trimEnd()
        val end = text.indexOfAny(charArrayOf(',', ')'), caret).let { if (it < 0) text.length else it }
        val formattedPrefix = if (prefix.endsWith("(") || prefix.endsWith(",")) "$prefix " else prefix
        val newArg = "\"$match\""
        return Edit(formattedPrefix + newArg + text.substring(end), formattedPrefix.length + newArg.length)
    }
}
