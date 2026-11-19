package com.cardputer.cardulator

import android.widget.LinearLayout

internal class HelpPanel(
    private val container: LinearLayout,
    private val cards: CalculatorMenuCards,
    private val dialogs: CalculatorDialogs
) {
    private var topics: List<Pair<String, String>>? = null

    fun render(engine: CardulatorEngine) {
        val entries = topics ?: loadTopics(engine).also { topics = it }
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

    private fun loadTopics(engine: CardulatorEngine): List<Pair<String, String>> {
        val topics = engine.nativeGetHelp("help(index)").lineSequence().filter { it.isNotBlank() }
        return topics.map { topic ->
            val detail = engine.nativeGetHelp("help($topic)").lineSequence()
                .filter { it.isNotBlank() && !it.startsWith("===") }.joinToString("\n")
            topic to detail
        }.toList()
    }
}
