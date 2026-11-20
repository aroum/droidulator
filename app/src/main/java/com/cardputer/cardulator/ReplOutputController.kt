package com.cardputer.cardulator

import android.widget.ScrollView
import android.widget.TextView
import java.util.concurrent.Future

internal class ReplOutputController(
    private val session: CalculatorSession,
    private val renderer: ReplOutputRenderer,
    private val output: TextView,
    private val scroll: ScrollView,
    private val welcome: () -> Pair<String, Boolean>,
    private val formatSettings: () -> Triple<Int, Int, Boolean>
) {
    private var generation = 0L
    private var pending: Future<*>? = null
    private var closed = false

    fun rebuild() {
        if (closed) return
        val version = ++generation
        pending?.cancel(false)
        val items = session.replDisplayItems.toList()
        val (message, showWelcome) = welcome()
        val (mode, precision, grouping) = formatSettings()
        pending = session.compute({
            nativeSetFormatSettings(mode, precision, grouping)
            ReplOutputRenderer.formatResults(items) { nativeFormatNumbers(it) }
        }) { result ->
            if (closed || version != generation) return@compute
            pending = null
            val formatted = result.getOrElse { items.map { it.rawOutput } }
            output.text = renderer.render(items, message, showWelcome, formatted)
            scrollToEnd()
        }
    }

    fun append(entry: ReplDisplayItem) {
        if (closed) return
        val items = session.replDisplayItems
        items.add(entry)
        val trimmed = items.size > 500
        if (trimmed) items.removeAt(0)
        if (trimmed || pending != null) {
            rebuild()
            return
        }
        if (entry.isRawText) output.append(entry.rawOutput)
        else output.append(renderer.entry(entry.input, entry.rawOutput, entry.isError))
        scrollToEnd()
    }

    fun close() {
        closed = true
        invalidate()
    }

    fun invalidate() {
        ++generation
        pending?.cancel(false)
        pending = null
    }

    private fun scrollToEnd() {
        scroll.post { if (!closed) scroll.fullScroll(ScrollView.FOCUS_DOWN) }
    }
}
