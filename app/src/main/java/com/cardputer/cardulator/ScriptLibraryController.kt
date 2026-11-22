package com.cardputer.cardulator

/** Keeps library mutations and native script registration in one place. */
internal class ScriptLibraryController(
    private val scripts: MutableList<Pair<String, String>>,
    private val register: (String, String) -> Unit,
    private val unregister: (String) -> Unit
) {
    fun save(originalName: String?, name: String, code: String): Boolean {
        val index = scripts.indexOfFirst { it.first == originalName }
        if (scripts.withIndex().any { it.index != index && it.value.first == name }) return false
        register(name, code)
        if (index >= 0) {
            val previous = scripts[index].first
            if (previous != name) unregister(previous)
            scripts[index] = name to code
        } else scripts.add(name to code)
        return true
    }

    fun remove(name: String): Boolean {
        val index = scripts.indexOfFirst { it.first == name }
        if (index < 0) return false
        unregister(name)
        scripts.removeAt(index)
        return true
    }

    fun clear() {
        scripts.toList().forEach { unregister(it.first) }
        scripts.clear()
    }
}
