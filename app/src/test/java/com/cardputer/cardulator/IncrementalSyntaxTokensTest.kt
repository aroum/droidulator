package com.cardputer.cardulator

import org.junit.Assert.*
import org.junit.Test

class IncrementalSyntaxTokensTest {
    @Test fun grammarKeepsNumbersFunctionsKeywordsAndConstantsDistinct() {
        val tokens = IncrementalSyntaxTokens.scan("sin(2)+pi xor e1")
        assertEquals(listOf(
            IncrementalSyntaxTokens.Kind.FUNCTION, IncrementalSyntaxTokens.Kind.NUMBER,
            IncrementalSyntaxTokens.Kind.OPERATION, IncrementalSyntaxTokens.Kind.CONSTANT,
            IncrementalSyntaxTokens.Kind.KEYWORD, IncrementalSyntaxTokens.Kind.CONSTANT
        ), tokens.map { it.kind })
    }

    @Test fun editingLastLineDoesNotRetokenizeFiveHundredUnchangedLines() {
        val cache = IncrementalSyntaxTokens()
        val prefix = (1..500).joinToString("\n", postfix = "\n") { "value_$it=sin($it)+$it" }
        cache.update(prefix + "value=1")
        val text = prefix + "value=12"
        assertEquals(IncrementalSyntaxTokens.scan(text), cache.update(text))
        assertEquals("value=12".length, cache.reparsedCharacters)
        cache.update(text)
        assertEquals(0, cache.reparsedCharacters)
    }

    @Test fun lineJoinsSplitsAndEntireTextReplacementKeepBoundaryTokensCorrect() {
        val cache = IncrementalSyntaxTokens()
        for (text in listOf("sin\n2", "sin2", "sin\n(2)", "1\n.5", "1.5", "", "e", "e1", "e\n1", "\nxor\n")) {
            assertEquals(text, IncrementalSyntaxTokens.scan(text), cache.update(text))
        }
    }

    @Test fun randomizedEditsMatchFullTokenization() {
        val random = java.util.Random(42)
        val text = StringBuilder((1..30).joinToString("\n") { "v$it=sin($it)+pi" })
        val cache = IncrementalSyntaxTokens()
        cache.update(text.toString())
        val inserts = listOf("", "\n", "\r\n", "2", "e-3", "sin", "xor", " ", ".5", "_v", "+", "\npi\n", "Ω", "🧮")
        repeat(2000) {
            val start = random.nextInt(text.length + 1)
            val end = (start + random.nextInt(12)).coerceAtMost(text.length)
            text.replace(start, end, inserts[random.nextInt(inserts.size)])
            val current = text.toString()
            assertEquals("edit $it: $current", IncrementalSyntaxTokens.scan(current), cache.update(current))
        }
    }

    @Test fun cachesDoNotShareEditorState() {
        val first = IncrementalSyntaxTokens()
        val second = IncrementalSyntaxTokens()
        first.update("sin(1)\npi")
        second.update("const x=4")
        assertEquals(IncrementalSyntaxTokens.scan("sin(2)\npi"), first.update("sin(2)\npi"))
        assertEquals(IncrementalSyntaxTokens.scan("const x=5"), second.update("const x=5"))
    }
}
