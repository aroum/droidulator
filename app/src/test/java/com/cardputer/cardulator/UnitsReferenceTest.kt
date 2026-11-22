package com.cardputer.cardulator

import org.junit.Assert.*
import org.junit.Test

class UnitsReferenceTest {
    @Test fun findsSymbolsNamesAndCommonSpelling() {
        val reference = UnitsReference(sequenceOf("meter m", "inch 2.54 cm # international inch"))
        for (query in listOf("mm", "millimeter", "millimetre", "milimeter")) {
            assertTrue(query, reference.search(query).any { it.name == "mm" && it.aliases.contains("millimeter") })
        }
        assertEquals("inch", reference.search("inch").first().name)
        assertEquals("2.54 cm", reference.search("inch").first().definition)
    }

    @Test fun preservesCaseAndIgnoresNonDefinitions() {
        val reference = UnitsReference(sequenceOf("# comment", "!include other.units", "meter m"))
        assertEquals("mm", reference.search("mm").first().name)
        assertEquals("Mm", reference.search("Mm").first().name)
        assertTrue(reference.search("other.units").isEmpty())
        assertTrue(reference.search("unfindable_unit_name").isEmpty())
        assertEquals(5, reference.search("", limit = 5).size)
    }
}
