package com.cardputer.cardulator

import android.content.Context
import android.widget.LinearLayout
import android.widget.TextView

internal class LibraryPanel(
    private val context: Context,
    private val container: LinearLayout,
    private val cards: CalculatorMenuCards
) {
    private val historyName = Regex("e[0-9]+")

    fun renderValues(
        values: Array<NativeValue>,
        constants: Boolean,
        showHistory: Boolean = true,
        onEdit: (String, String) -> Unit,
        onDelete: (String) -> Unit
    ) {
        val entries = values.filter { constants || showHistory || !historyName.matches(it.name) }
        if (entries.isEmpty() && !constants) {
            showEmpty(R.string.ui_text_005)
            return
        }
        for (entry in entries) {
            val name = entry.name
            val value = entry.expressionValue()
            val builtIn = constants && (name == "pi" || name == "e")
            val readOnly = builtIn || (!constants && historyName.matches(name))
            val subtitle = if (constants) {
                "= $value ${if (builtIn) context.getString(R.string.builtin_constant) else ""}"
            } else "= $value"
            container.addView(cards.create(
                title = name,
                subtitle = subtitle,
                cardBgColor = ThemePalette.bgCard,
                textColor = ThemePalette.fgText,
                dimColor = ThemePalette.fgDim,
                onEdit = if (readOnly) null else ({ onEdit(name, "$name = $value") }),
                onDelete = if (readOnly) null else ({ onDelete(name) })
            ))
        }
    }

    fun renderFormulas(
        formulas: Array<NativeFormula>,
        onRun: (NativeFormula) -> Unit,
        onEdit: (NativeFormula) -> Unit,
        onDelete: (NativeFormula) -> Unit
    ) {
        if (formulas.isEmpty()) {
            showEmpty(R.string.ui_text_006)
            return
        }
        for (formula in formulas) {
            container.addView(cards.create(
                title = formula.signature,
                subtitle = "= ${formula.body}",
                cardBgColor = ThemePalette.bgCard,
                textColor = ThemePalette.fgText,
                dimColor = ThemePalette.fgDim,
                onRun = { onRun(formula) },
                onEdit = { onEdit(formula) },
                onDelete = { onDelete(formula) }
            ))
        }
    }

    fun renderScripts(
        scripts: List<Pair<String, String>>,
        onRun: (String, String) -> Unit,
        onEdit: (String) -> Unit,
        onDelete: (String) -> Unit
    ) {
        if (scripts.isEmpty()) {
            showEmpty(R.string.ui_text_007)
            return
        }
        for ((name, code) in scripts) {
            val card = cards.create(
                title = name,
                subtitle = code.lineSequence().firstOrNull().orEmpty().take(60),
                runLabel = context.getString(R.string.action_run),
                cardBgColor = ThemePalette.bgCard,
                textColor = ThemePalette.fgText,
                dimColor = ThemePalette.fgDim,
                onRun = { onRun(code, name) },
                onEdit = { onEdit(name) },
                onDelete = { onDelete(name) }
            )
            card.tag = "$name $code"
            container.addView(card)
        }
    }

    private fun showEmpty(message: Int) {
        val horizontal = (16 * context.resources.displayMetrics.density + 0.5f).toInt()
        val vertical = (32 * context.resources.displayMetrics.density + 0.5f).toInt()
        container.addView(TextView(context).apply {
            setText(message)
            setTextColor(ThemePalette.fgDim)
            textSize = 14f
            setPadding(horizontal, vertical, horizontal, vertical)
        })
    }
}
