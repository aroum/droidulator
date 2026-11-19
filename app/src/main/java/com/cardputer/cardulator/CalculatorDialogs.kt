package com.cardputer.cardulator

import android.app.Activity
import android.app.Dialog
import android.content.res.ColorStateList
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView

internal class CalculatorDialogs(
    private val activity: Activity,
    private val trackDialog: (Dialog) -> Dialog
) {
    private fun dp(value: Int): Int =
        (value * activity.resources.displayMetrics.density + 0.5f).toInt()

    fun showScriptOutput(title: String, logText: String) {
        val bgSoft = ThemePalette.bgSoft
        val fgColor = ThemePalette.fgText
        val borderColor = ThemePalette.borderColor

        val tvOut = TextView(activity).apply {
            text = if (logText.isBlank()) activity.getString(R.string.script_output_empty) else logText
            setTextColor(fgColor)
            textSize = 13f
            typeface = android.graphics.Typeface.MONOSPACE
            setPadding(dp(20), dp(16), dp(20), dp(16))
        }

        val scrollContainer = ScrollView(activity).apply {
            val bgDrawable = GradientDrawable().apply {
                setColor(bgSoft)
                setStroke(dp(2), borderColor)
                cornerRadius = dp(8).toFloat()
            }
            background = bgDrawable
            isScrollbarFadingEnabled = false
            val lp = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(400))
            lp.setMargins(0, dp(8), 0, dp(8))
            layoutParams = lp
            addView(tvOut)
        }

        show(title, scrollContainer, positiveText = activity.getString(R.string.ui_text_015), showCancel = false) {}
    }

    fun show(
        title: String,
        contentView: View,
        positiveText: String? = activity.getString(R.string.action_save),
        showCancel: Boolean = true,
        customButtons: List<View>? = null,
        positiveValidator: (() -> Boolean)? = null,
        onPositive: (() -> Unit)? = null
    ): Dialog {
        val dialog = trackDialog(Dialog(activity, androidx.appcompat.R.style.Theme_AppCompat_DayNight_NoActionBar))
        val bgColor = ThemePalette.bg
        val bgSoft = ThemePalette.bgSoft
        val fgColor = ThemePalette.fgText
        val btnAqua = ThemePalette.aqua
        val borderColor = ThemePalette.borderColor

        dialog.window?.apply {
            setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            val dialogDrawable = GradientDrawable().apply {
                setColor(bgColor)
                setStroke(dp(2), borderColor)
                cornerRadius = dp(8).toFloat()
            }
            setBackgroundDrawable(dialogDrawable)
            setDimAmount(0.6f)
            addFlags(android.view.WindowManager.LayoutParams.FLAG_DIM_BEHIND)
            if (android.os.Build.VERSION.SDK_INT < 35) statusBarColor = bgSoft
            val controller = androidx.core.view.WindowCompat.getInsetsController(this, decorView)
            controller.isAppearanceLightStatusBars = androidx.core.graphics.ColorUtils.calculateLuminance(bgSoft) > 0.5
        }

        val root = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(bgColor)
            setPadding(dp(36), dp(32), dp(36), dp(32))
        }

        val tvTitle = TextView(activity).apply {
            text = title
            setTextColor(fgColor)
            textSize = 16f
            typeface = android.graphics.Typeface.MONOSPACE
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            setPadding(0, 0, 0, dp(16))
        }

        root.addView(tvTitle)

        (contentView.parent as? ViewGroup)?.removeView(contentView)
        if (contentView is ScrollView) {
            root.addView(contentView, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        } else {
            root.addView(contentView)
        }

        val btnBar = LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = android.view.Gravity.END
            setPadding(0, dp(20), 0, 0)
        }

        if (customButtons != null) {
            for (btn in customButtons) {
                if (btn.parent != null) {
                    (btn.parent as ViewGroup).removeView(btn)
                }
                btnBar.addView(btn)
            }
        } else {
            if (showCancel) {
                val btnCancel = Button(activity).apply {
                    text = activity.getString(R.string.ui_text_011)
                    setTextColor(fgColor)
                    setBackgroundColor(Color.TRANSPARENT)
                    typeface = android.graphics.Typeface.MONOSPACE
                    textSize = 13f
                    setOnClickListener { dialog.dismiss() }
                }
                btnBar.addView(btnCancel)
            }

            if (positiveText != null) {
                val btnOk = Button(activity).apply {
                    text = positiveText
                    setTextColor(ThemePalette.fgInverted)
                    backgroundTintList = ColorStateList.valueOf(btnAqua)
                    typeface = android.graphics.Typeface.MONOSPACE
                    textSize = 13f
                    setTypeface(typeface, android.graphics.Typeface.BOLD)
                    val lp = LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT)
                    if (showCancel) lp.setMargins(dp(16), 0, 0, 0)
                    layoutParams = lp
                    setOnClickListener {
                        if (positiveValidator?.invoke() == false) return@setOnClickListener
                        onPositive?.invoke()
                        dialog.dismiss()
                    }
                }
                btnBar.addView(btnOk)
            }
        }

        root.addView(btnBar)

        dialog.setContentView(root)
        dialog.show()
        if (contentView is ScrollView) {
            root.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
            val screen = activity.resources.displayMetrics
            dialog.window?.setLayout(screen.widthPixels - dp(32), (screen.heightPixels * 0.8f).toInt())
        }
        return dialog
    }
}
