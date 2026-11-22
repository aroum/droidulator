package com.cardputer.cardulator

import android.app.Activity
import android.app.Dialog
import android.content.res.ColorStateList
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast

internal class FormulaRunner(
    private val activity: Activity,
    private val session: CalculatorSession,
    private val dialogs: CalculatorDialogs,
    private val queueCommand: (String) -> Boolean,
    private val untrack: (android.content.DialogInterface) -> Unit
) {
    private fun dp(value: Int): Int =
        (value * activity.resources.displayMetrics.density + 0.5f).toInt()

    fun show(formula: NativeFormula, arguments: List<String> = emptyList()) {
        val formulaLine = formula.expression
        val cleanName = formula.name
        val params = formula.parameters

        val bgSoft = ThemePalette.bgSoft
        val fgColor = ThemePalette.fgText
        val fgDim = ThemePalette.fgDim
        val borderColor = ThemePalette.borderColor

        val layout = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
        }

        val inputFields = mutableListOf<Pair<String, EditText>>()

        for ((index, param) in params.withIndex()) {
            val et = dialogs.formulaArgument(param, arguments.getOrNull(index).orEmpty())
            layout.addView(et)
            inputFields.add(Pair(param, et))
        }

        val tvResultLabel = TextView(activity).apply {
            text = activity.getString(R.string.ui_text_012)
            setTextColor(fgDim)
            textSize = 12f
            typeface = android.graphics.Typeface.MONOSPACE
            setPadding(0, dp(8), 0, dp(4))
        }

        val tvResultValue = TextView(activity).apply {
            text = activity.getString(R.string.ui_text_013)
            setTextColor(fgColor)
            textSize = 15f
            typeface = android.graphics.Typeface.MONOSPACE
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            setPadding(dp(20), dp(14), dp(20), dp(14))
            val bgDrawable = GradientDrawable().apply {
                setColor(bgSoft)
                setStroke(dp(2), borderColor)
                cornerRadius = dp(8).toFloat()
            }
            background = bgDrawable
        }

        var currentEvalExpr = ""
        var currentRes = ""
        var previewVersion = 0
        var scheduledPreview: Runnable? = null
        var runningPreview: java.util.concurrent.Future<*>? = null

        fun cancelPreview() {
            previewVersion++
            scheduledPreview?.let { tvResultValue.removeCallbacks(it) }
            scheduledPreview = null
            runningPreview?.cancel(false)
            runningPreview = null
        }

        fun updateLiveResult() {
            cancelPreview()
            val draftArguments = inputFields.map { it.second.text.toString() }
            session.formulaRunDraft = FormulaRunDraft(formula, draftArguments)
            val argValues = draftArguments.map { it.trim() }
            val hasEmptyInput = argValues.any { it.isEmpty() }
            if (hasEmptyInput && params.isNotEmpty()) {
                currentEvalExpr = if (argValues.isEmpty()) "$cleanName()" else "$cleanName(${argValues.joinToString(", ")})"
                currentRes = activity.getString(R.string.enter_arguments)
                tvResultValue.text = currentRes
                return
            }

            currentEvalExpr = if (argValues.isEmpty()) "$cleanName()" else "$cleanName(${argValues.joinToString(", ")})"
            val expression = currentEvalExpr
            val version = previewVersion
            currentRes = ""
            tvResultValue.text = expression
            val task = Runnable {
                runningPreview = session.compute({ nativeEvaluatePreview(expression) }) { result ->
                    if (version == previewVersion && !activity.isDestroyed && tvResultValue.isAttachedToWindow) {
                        currentRes = result.getOrElse { it.localizedMessage.orEmpty() }
                        tvResultValue.text = "$expression = $currentRes"
                    }
                }
            }
            scheduledPreview = task
            tvResultValue.postDelayed(task, 150)
        }

        for (pair in inputFields) {
            pair.second.addTextChangedListener(object : android.text.TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
                    updateLiveResult()
                }
                override fun afterTextChanged(s: android.text.Editable?) {}
            })
        }

        layout.addView(tvResultLabel)
        layout.addView(tvResultValue)

        updateLiveResult()

        val btnExport = Button(activity).apply {
            text = activity.getString(R.string.ui_text_014)
            setTextColor(ThemePalette.fgInverted)
            backgroundTintList = ColorStateList.valueOf(ThemePalette.aqua)
            typeface = android.graphics.Typeface.MONOSPACE
            textSize = 13f
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            val lp = LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            lp.setMargins(dp(16), 0, 0, 0)
            layoutParams = lp
        }

        var formulaDialog: Dialog? = null

        val btnClose = Button(activity).apply {
            text = activity.getString(R.string.ui_text_015)
            setTextColor(fgColor)
            setBackgroundColor(Color.TRANSPARENT)
            typeface = android.graphics.Typeface.MONOSPACE
            textSize = 13f
            setOnClickListener { formulaDialog?.dismiss() }
        }

        btnExport.setOnClickListener {
            if (currentEvalExpr.isNotEmpty() && currentRes.isNotEmpty() && currentRes != activity.getString(R.string.enter_arguments)) {
                if (queueCommand(currentEvalExpr)) {
                    Toast.makeText(activity, R.string.ui_text_016, Toast.LENGTH_SHORT).show()
                }
            }
        }

        formulaDialog = dialogs.show(
            formulaLine,
            layout,
            customButtons = listOf(btnClose, btnExport)
        )
        formulaDialog.setOnDismissListener { dismissed ->
            cancelPreview()
            if (!activity.isChangingConfigurations) session.formulaRunDraft = null
            untrack(dismissed)
        }
    }

}
