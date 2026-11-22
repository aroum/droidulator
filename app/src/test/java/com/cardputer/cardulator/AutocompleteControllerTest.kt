package com.cardputer.cardulator

import org.junit.Assert.*
import org.junit.Test

class AutocompleteControllerTest {
    @Test fun cyclesUnitsAndInvalidatesOnManualEdits() {
        var calls = 0
        val controller = AutocompleteController({ _, prefix ->
            calls++
            if (prefix == "m") listOf("mm", "meter") else listOf("cm")
        }, { emptyList() }, { _, _ -> null })
        val first = controller.complete("conv(1,m)", 8)!!
        assertEquals("conv(1, \"mm\")", first.text)
        val second = controller.complete(first.text, first.caret)!!
        assertEquals("conv(1, \"meter\")", second.text)
        assertEquals(1, calls)
        val edited = controller.complete("conv(1,c)", 8)!!
        assertEquals("conv(1, \"cm\")", edited.text)
        assertEquals(2, calls)
    }

    @Test fun preservesSuffixAndDelegatesGeneralCompletion() {
        val controller = AutocompleteController({ _, _ -> emptyList() }, { listOf("factorial") },
            { text, caret -> AutocompleteController.Edit(text.substring(0, caret) + "n" + text.substring(caret), caret + 1) })
        assertEquals("run(\"factorial\")+2", controller.complete("run(f)+2", 6)?.text)
        assertEquals("sin(x)", controller.complete("si(x)", 2)?.text)
        assertNull(controller.complete("x", 2))
    }
}
