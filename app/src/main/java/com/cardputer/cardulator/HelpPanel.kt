package com.cardputer.cardulator

import android.widget.LinearLayout

internal class HelpPanel(
    private val container: LinearLayout,
    private val cards: CalculatorMenuCards,
    private val dialogs: CalculatorDialogs
) {
    fun render(engine: CardulatorEngine) {
        val topics = engine.nativeGetHelp("help(index)").lineSequence().filter { it.isNotBlank() }
        for (topic in topics) {
            val detail = engine.nativeGetHelp("help($topic)").lineSequence()
                .filter { it.isNotBlank() && !it.startsWith("===") }.joinToString("\n")
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
}
