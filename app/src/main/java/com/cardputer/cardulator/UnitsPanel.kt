package com.cardputer.cardulator

import android.content.Context
import android.graphics.Typeface
import android.widget.LinearLayout
import android.widget.TextView

internal class UnitsPanel(
    private val context: Context,
    private val container: LinearLayout,
    private val cards: CalculatorMenuCards,
    private val dialogs: CalculatorDialogs
) {
    private val reference by lazy {
        context.assets.open("units.dat").bufferedReader().use { UnitsReference(it.lineSequence()) }
    }

    private fun dp(value: Int): Int =
        (value * context.resources.displayMetrics.density + 0.5f).toInt()

    fun render(query: String) {
        container.removeAllViews()
        container.addView(TextView(context).apply {
            text = context.getString(R.string.units_search_hint)
            setTextColor(ThemePalette.fgDim)
            typeface = Typeface.MONOSPACE
            setPadding(dp(8), dp(8), dp(8), dp(8))
        })
        for (entry in reference.search(query)) {
            val detail = context.getString(
                R.string.unit_reference_detail, entry.name, entry.aliases, entry.definition, entry.name
            )
            val card = cards.create(
                entry.name, entry.aliases.ifEmpty { entry.definition },
                ThemePalette.bgCard, ThemePalette.fgText, ThemePalette.fgDim,
                onEdit = null, onDelete = null
            )
            card.setOnClickListener { dialogs.showHelpDetail(entry.name, detail) }
            container.addView(card)
        }
    }
}
