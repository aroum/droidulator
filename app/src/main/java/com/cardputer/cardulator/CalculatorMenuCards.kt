package com.cardputer.cardulator

import android.content.Context
import android.content.res.ColorStateList
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView

internal class CalculatorMenuCards(private val context: Context) {
    private fun dp(value: Int): Int =
        (value * context.resources.displayMetrics.density + 0.5f).toInt()

    fun create(
        title: String,
        subtitle: String,
        cardBgColor: Int,
        textColor: Int,
        dimColor: Int,
        onRun: (() -> Unit)? = null,
        runLabel: String = context.getString(R.string.action_eval),
        onEdit: (() -> Unit)?,
        onDelete: (() -> Unit)?
    ): View {
        val container = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = android.view.Gravity.CENTER_VERTICAL
            background = GradientDrawable().apply { setColor(cardBgColor); cornerRadius = dp(8).toFloat() }
            tag = "$title $subtitle"
            setPadding(dp(6), dp(2), dp(6), dp(2))
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { bottomMargin = dp(3) }
        }
        val labels = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(0, -2, 1f)
        }
        fun label(value: String, size: Float, color: Int) = TextView(context).apply {
            text = value
            textSize = size
            setTextColor(color)
            typeface = android.graphics.Typeface.MONOSPACE
            maxLines = 1
            ellipsize = android.text.TextUtils.TruncateAt.END
        }
        labels.addView(label(title, 15f, textColor))
        labels.addView(label(subtitle, 13f, dimColor))
        container.addView(labels)
        fun action(icon: Int, description: String, color: Int, callback: () -> Unit) {
            container.addView(com.google.android.material.button.MaterialButton(context).apply {
                layoutParams = LinearLayout.LayoutParams(dp(36), dp(36)).apply { marginStart = dp(4) }
                contentDescription = description
                setIconResource(icon)
                iconSize = dp(20)
                iconPadding = 0
                iconGravity = com.google.android.material.button.MaterialButton.ICON_GRAVITY_TEXT_START
                iconTint = ColorStateList.valueOf(ThemePalette.bgDarkText)
                backgroundTintList = ColorStateList.valueOf(color)
                cornerRadius = dp(8)
                setPadding(0, 0, 0, 0)
                setOnClickListener { callback() }
            })
        }
        onRun?.let { action(R.drawable.ic_run, runLabel, ThemePalette.yellow, it) }
        onEdit?.let { action(R.drawable.ic_edit, context.getString(R.string.ui_text_009), ThemePalette.green, it) }
        onDelete?.let { action(R.drawable.ic_close, context.getString(R.string.ui_text_010), ThemePalette.red, it) }

        return container
    }

}
