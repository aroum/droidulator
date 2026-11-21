package com.cardputer.cardulator

import android.widget.LinearLayout
import java.util.concurrent.Future

internal class HelpPanel(
    private val container: LinearLayout,
    private val cards: CalculatorMenuCards,
    private val dialogs: CalculatorDialogs,
    private val session: CalculatorSession
) {
    private var generation = 0L
    private var pending: Future<*>? = null
    private var closed = false

    fun render(onRendered: () -> Unit) {
        cancel()
        if (closed) return
        session.helpTopics?.let {
            draw(it)
            onRendered()
            return
        }
        val version = generation
        pending = session.compute({ loadTopics(this) }) { result ->
            if (closed || version != generation) return@compute
            pending = null
            result.onSuccess { entries ->
                session.helpTopics = entries
                draw(entries)
                onRendered()
            }
        }
    }

    fun cancel() {
        ++generation
        pending?.cancel(false)
        pending = null
    }

    fun close() {
        closed = true
        cancel()
    }

    private fun draw(entries: List<Pair<String, String>>) {
        container.removeAllViews()
        for ((topic, detail) in entries) {
            val card = cards.create(
                topic, detail.lineSequence().firstOrNull().orEmpty(),
                ThemePalette.bgCard, ThemePalette.fgText, ThemePalette.fgDim,
                onEdit = null, onDelete = null
            )
            card.tag = "$topic $detail"
            card.setOnClickListener { dialogs.showHelpDetail(topic, detail) }
            container.addView(card)
        }
    }

    private companion object {
        private fun loadTopics(engine: CardulatorEngine): List<Pair<String, String>> {
            val topics = engine.nativeGetHelp("help(index)").lineSequence().filter { it.isNotBlank() }
            return topics.map { topic ->
                val detail = engine.nativeGetHelp("help($topic)").lineSequence()
                    .filter { it.isNotBlank() && !it.startsWith("===") }.joinToString("\n")
                topic to detail
            }.toList()
        }
    }
}
