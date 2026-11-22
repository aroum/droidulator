package com.cardputer.cardulator

/** Searchable names and definitions from the bundled GNU Units database. */
class UnitsReference(lines: Sequence<String>) {
    data class Entry(val name: String, val definition: String, val aliases: String = "")

    private val entries: List<Entry>

    init {
        val definitions = linkedMapOf<String, Entry>()
        val whitespace = Regex("\\s+")
        val unitName = Regex("[A-Za-z][A-Za-z0-9_]*")
        for (line in lines) {
            val body = line.substringBefore('#').trim()
            val parts = body.split(whitespace, limit = 2)
            if (parts.size != 2 || !parts[0].matches(unitName)) continue
            definitions.putIfAbsent(parts[0], Entry(parts[0], parts[1]))
        }
        val prefixes = listOf("k" to "kilo", "M" to "mega", "G" to "giga", "T" to "tera",
            "m" to "milli", "c" to "centi", "d" to "deci", "u" to "micro",
            "n" to "nano", "p" to "pico", "f" to "femto", "h" to "hecto", "da" to "deca")
        val bases = listOf("m" to "meter metre", "g" to "gram", "s" to "second", "A" to "ampere",
            "V" to "volt", "ohm" to "ohm", "F" to "farad", "H" to "henry", "W" to "watt",
            "Hz" to "hertz", "L" to "liter litre", "Pa" to "pascal", "J" to "joule")
        for ((symbol, prefix) in prefixes) for ((base, names) in bases) {
            val code = symbol + base
            val aliases = names.split(' ').joinToString(" ") { prefix + it }
            definitions.putIfAbsent(code, Entry(code, "$prefix-$base", aliases))
        }
        entries = definitions.values.sortedBy { it.name.lowercase() }
    }

    fun search(query: String, limit: Int = 100): List<Entry> {
        fun normalize(value: String) = value.lowercase().replace("milimeter", "millimeter")
            .replace("milimetre", "millimetre")
        val words = normalize(query).trim().split(Regex("\\s+")).filter { it.isNotEmpty() }
        return entries.asSequence().filter { entry ->
            val text = normalize("${entry.name} ${entry.aliases} ${entry.definition}")
            words.all { text.contains(it) }
        }.sortedBy { if (it.name == query.trim()) 0 else if (normalize(it.name) == normalize(query.trim())) 1 else 2 }
            .take(limit).toList()
    }
}
