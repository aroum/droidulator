package com.cardputer.cardulator

import org.junit.Assert.*
import org.junit.Test

class ScriptLibraryControllerTest {
    @Test fun savesRenamesAndRejectsDuplicateNamesWithoutNativeChanges() {
        val scripts = mutableListOf("first" to "1", "second" to "2")
        val registered = mutableListOf<Pair<String, String>>()
        val removed = mutableListOf<String>()
        val controller = ScriptLibraryController(scripts, { name, code -> registered.add(name to code) }, { removed.add(it) })
        assertFalse(controller.save("first", "second", "3"))
        assertTrue(registered.isEmpty() && removed.isEmpty())
        assertTrue(controller.save("first", "renamed", "3"))
        assertEquals(listOf("renamed" to "3", "second" to "2"), scripts)
        assertEquals(listOf("renamed" to "3"), registered)
        assertEquals(listOf("first"), removed)
        assertTrue(controller.save(null, "new", "4"))
        assertTrue(controller.remove("second"))
        assertFalse(controller.remove("missing"))
        controller.clear()
        assertTrue(scripts.isEmpty())
        assertEquals(listOf("first", "second", "renamed", "new"), removed)
    }
}
