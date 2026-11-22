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
    private val scrollToEndTask = Runnable { if (!closed) scroll.smoothScrollTo(0, output.bottom) }

    fun rebuild() {
        if (closed) return
        val version = ++generation
        pending?.cancel(false)
        val items = session.replDisplayItems.toList()
        val (message, showWelcome) = welcome()
        val (mode, precision, grouping) = formatSettings()
        val palette = ReplOutputRenderer.Palette.capture()
        val snapshotRenderer = renderer
        pending = session.compute({
            performanceTrace("Droidulator.replRebuild") {
                nativeSetFormatSettings(mode, precision, grouping)
                val formatted = ReplOutputRenderer.formatResults(items) { nativeFormatNumbers(it) }
                snapshotRenderer.render(items, message, showWelcome, formatted, palette)
            }
        }) { result ->
            if (closed || version != generation) return@compute
            pending = null
            output.text = result.getOrElse {
                renderer.render(items, message, showWelcome, items.map { it.rawOutput }, palette)
            }
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
        scroll.removeCallbacks(scrollToEndTask)
        invalidate()
    }

    fun invalidate() {
        ++generation
        pending?.cancel(false)
        pending = null
    }

    private fun scrollToEnd() {
        scroll.post(scrollToEndTask)
    }
}
