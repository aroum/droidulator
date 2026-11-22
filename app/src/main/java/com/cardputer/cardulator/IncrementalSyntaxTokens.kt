package com.cardputer.cardulator

/** Caches tokenization of masked code; callers mask strings/comments before updating. */
internal class IncrementalSyntaxTokens {
    enum class Kind { NUMBER, KEYWORD, FUNCTION, CONSTANT, OPERATION }
    data class Token(val start: Int, val end: Int, val kind: Kind)

    private var previousCode: String? = null
    private var previousTokens: List<Token> = emptyList()
    var reparsedCharacters: Int = 0
        private set

    fun update(code: String): List<Token> {
        val old = previousCode
        if (old == null) return remember(code, scan(code), code.length)
        var prefix = 0
        val commonLength = minOf(old.length, code.length)
        while (prefix < commonLength && old[prefix] == code[prefix]) prefix++
        if (prefix == old.length && prefix == code.length) {
            reparsedCharacters = 0
            return previousTokens
        }
        var suffix = 0
        while (suffix < commonLength - prefix &&
            old[old.lastIndex - suffix] == code[code.lastIndex - suffix]) suffix++

        // Whole lines preserve regex word boundaries and lookbehind/lookahead context.
        val start = code.lastIndexOf('\n', prefix - 1) + 1
        val oldChangedEnd = old.length - suffix
        val newline = old.indexOf('\n', oldChangedEnd)
        val oldEnd = if (newline < 0) old.length else newline + 1
        val delta = code.length - old.length
        val end = oldEnd + delta
        val updated = ArrayList<Token>(previousTokens.size)
        previousTokens.filterTo(updated) { it.end <= start }
        updated.addAll(scan(code.substring(start, end), start))
        for (token in previousTokens) {
            if (token.start >= oldEnd) updated.add(
                if (delta == 0) token else token.copy(start = token.start + delta, end = token.end + delta)
            )
        }
        return remember(code, updated, end - start)
    }

    private fun remember(code: String, tokens: List<Token>, count: Int): List<Token> {
        previousCode = code
        previousTokens = tokens
        reparsedCharacters = count
        return tokens
    }

    companion object {
        fun scan(code: String, offset: Int = 0): List<Token> = SYNTAX_REGEX.findAll(code).map { match ->
            val kind = when {
                match.groups[1] != null -> Kind.NUMBER
                match.groups[2] != null -> Kind.KEYWORD
                match.groups[3] != null -> Kind.FUNCTION
                match.groups[4] != null -> Kind.CONSTANT
                else -> Kind.OPERATION
            }
            Token(offset + match.range.first, offset + match.range.last + 1, kind)
        }.toList()
        // Each alternative owns one capture group, so classification needs no second regex match.
        private val NUMBER_REGEX = Regex("(?<![A-Za-z0-9_])(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?[a-zA-Z]*(?![A-Za-z0-9_])")
        private val KEYWORD_REGEX = Regex("\\b(?:for|while|if|iff|else|return|const|fn|def|function|and|or|not|xor)\\b")
        private val FUNC_REGEX = Regex("\\b(?:sin|cos|tan|tg|cot|ctg|ctan|sec|csc|cosec|asin|arcsin|acos|arccos|atan|arctan|atg|arctg|atan2|acot|arccot|actg|arcctg|actan|arcctan|asec|arcsec|acsc|arccsc|acosec|arccosec|sinh|sh|cosh|ch|tanh|th|coth|cth|ctgh|ctanh|sech|sch|csch|cosech|asinh|ahsin|arcsinh|arsinh|arsh|acosh|ahcos|arccosh|arcosh|arch|atanh|ahtan|arctanh|artanh|arth|artg|arctgh|acoth|ahcot|arccoth|arcoth|arcth|ahcoth|asech|ahsec|arcsech|arsech|ahsech|acsch|ahcsc|arccsch|arcsch|acosech|arccosech|ahcsch|deg2rad|d2r|rad2deg|r2d|sqrt|ln|log|log10|log2|logb|exp|cbrt|abs|ceil|floor|round|trunc|sgn|mod|mean|median|mode|var|std|sum|prod|min|max|dot|C|Cnk|P|rUni|rNor|fact|gcd|lcm|fib|len|print|run|plot|conv|help)\\b")
        private val CONST_REGEX = Regex("\\b(?:pi|phi|e\\d+)\\b|(?<![a-zA-Z0-9_])e(?![a-zA-Z0-9_])")
        private val OP_REGEX = Regex("[+\\-*/^=><!%]")
        private val SYNTAX_REGEX = Regex("(${NUMBER_REGEX.pattern})|(${KEYWORD_REGEX.pattern})|(${FUNC_REGEX.pattern})|(${CONST_REGEX.pattern})|(${OP_REGEX.pattern})")
    }
}
