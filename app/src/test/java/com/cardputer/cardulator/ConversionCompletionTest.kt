package com.cardputer.cardulator

import org.junit.Assert.*
import org.junit.Test

class ConversionCompletionTest {
    private fun context(text: String) = ConversionCompletion.contextBeforeCaret(text, text.length)

    @Test fun recognizesCallsAndWhitespace() {
        assertEquals(listOf("1", " m", " c"), context("conv(1, m, c")?.second)
        assertEquals(listOf("1", " m", " c"), context("conv \t(1, m, c")?.second)
        assertNull(context("myconv(1, m"))
        assertNull(context("_conv(1, m"))
        assertNull(context("conv(1, m)"))
    }

    @Test fun preservesNestedArgumentsAndQuotedCommas() {
        assertEquals(listOf("[1,2]", " m", " c"), context("conv([1,2], m, c")?.second)
        assertEquals(listOf("max(1,2)", " 'm,s'", " c"), context("conv(max(1,2), 'm,s', c")?.second)
        assertEquals(listOf("1", " \"m\\\",s\"", " c"), context("conv(1, \"m\\\",s\", c")?.second)
        assertNull(context("conv(max(1, "))
        assertNull(context("conv([1, "))
        assertNull(context("conv([1,2), m"))
    }

    @Test fun replacesOnlyTheArgumentBeforeCaret() {
        val text = "conv(1, m, cm)"
        val caret = text.indexOf("cm") + 1
        val result = ConversionCompletion.contextBeforeCaret(text, caret)!!
        assertEquals(" c", text.substring(result.first, caret))
        assertEquals(listOf("1", " m", " c"), result.second)
        assertNull(ConversionCompletion.contextBeforeCaret(text, -1))
        assertNull(ConversionCompletion.contextBeforeCaret(text, text.length + 1))
    }
}
