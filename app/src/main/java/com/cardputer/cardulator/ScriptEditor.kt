package com.cardputer.cardulator

import android.app.Activity
import android.app.Dialog
import android.content.res.ColorStateList
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.GradientDrawable
import android.text.Editable
import android.text.TextWatcher
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.core.widget.doAfterTextChanged

internal class ScriptEditor(
    private val activity: Activity,
    private val session: CalculatorSession,
    private val highlighter: CalculatorSyntaxHighlighter,
    private val applySystemInsets: (View) -> Unit,
    private val selectAutocompleteEditor: (EditText) -> Unit,
    private val queueCommand: (String, String) -> Boolean,
    private val onOpen: (Dialog) -> Unit,
    private val onDismiss: (Dialog) -> Unit
) {
    private fun rounded(color: Int) = GradientDrawable().apply {
        setColor(color)
        cornerRadius = 8 * activity.resources.displayMetrics.density
    }

    fun show(draft: ScriptDraft, onSave: (String, String) -> Boolean) {
        val dialog = Dialog(activity, R.style.GruvboxFullscreenDialog)
        dialog.setContentView(R.layout.dialog_script_editor)

        val root = dialog.findViewById<LinearLayout>(R.id.script_editor_root)
        applySystemInsets(root)
        val btnNavLeft = dialog.findViewById<Button>(R.id.btn_script_nav_left)
        val btnNavRight = dialog.findViewById<Button>(R.id.btn_script_nav_right)
        val btnNavUp = dialog.findViewById<Button>(R.id.btn_script_nav_up)
        val btnNavDown = dialog.findViewById<Button>(R.id.btn_script_nav_down)
        val btnTab = dialog.findViewById<com.google.android.material.button.MaterialButton>(R.id.btn_script_tab)
        val btnCancel = dialog.findViewById<Button>(R.id.btn_script_cancel)
        val btnRun = dialog.findViewById<Button>(R.id.btn_script_run)
        val btnSave = dialog.findViewById<com.google.android.material.button.MaterialButton>(R.id.btn_script_save)
        val etName = dialog.findViewById<EditText>(R.id.et_script_name)
        val etCode = dialog.findViewById<EditText>(R.id.et_script_code)

        val bgColor = ThemePalette.bg
        val bgSoft = ThemePalette.bgSoft
        val fgColor = ThemePalette.fgText
        val fgDim = ThemePalette.fgDim
        val btnAqua = ThemePalette.aqua

        dialog.findViewById<View>(R.id.script_editor_header).background = rounded(bgSoft)
        dialog.findViewById<View>(R.id.script_editor_toolbar).background = rounded(bgSoft)
        root.setBackgroundColor(bgColor)

        etName.apply {
            setText(draft.name)
            setSelection(draft.nameCursor.coerceIn(0, length()))
            doAfterTextChanged { draft.name = it.toString() }
            background = rounded(bgSoft)
            setTextColor(fgColor)
            setHintTextColor(fgDim)
        }

        val lineLabel = dialog.findViewById<TextView>(R.id.script_line)
        lineLabel.setTextColor(fgColor)
        var lastCursor = -1
        var lastEdit = -1
        var revision = 0
        etCode.doAfterTextChanged { revision++ }
        etCode.viewTreeObserver.addOnPreDrawListener {
            val cursor = etCode.selectionStart.coerceIn(0, etCode.length())
            if (cursor != lastCursor || revision != lastEdit) {
                var line = 1
                for (i in 0 until cursor) if (etCode.text[i] == '\n') line++
                lineLabel.text = activity.getString(R.string.line_number, line)
                lastCursor = cursor
                lastEdit = revision
            }
            true
        }
        etCode.apply {
            setText(draft.code)
            setSelection(draft.codeCursor.coerceIn(0, length()))
            doAfterTextChanged { draft.code = it.toString() }
            background = rounded(bgSoft)
            setTextColor(fgColor)
            setHintTextColor(fgDim)
            val editor = this
            doAfterTextChanged { text ->
                if (text != null && !highlighter.isApplying(text)) highlighter.apply(editor.editableText)
            }
        }

        if (draft.code.isNotEmpty()) {
            highlighter.apply(etCode.editableText)
        }

        val btnFg = ThemePalette.btnText

        btnNavLeft.apply {
            backgroundTintList = ColorStateList.valueOf(ThemePalette.blue)
            setTextColor(btnFg)
            setToolbarClickListener {
                val sel = etCode.selectionStart
                if (sel > 0) etCode.setSelection(sel - 1)
                etCode.requestFocus()
            }
        }

        btnNavRight.apply {
            backgroundTintList = ColorStateList.valueOf(ThemePalette.blue)
            setTextColor(btnFg)
            setToolbarClickListener {
                val sel = etCode.selectionStart
                if (sel < etCode.text.length) etCode.setSelection(sel + 1)
                etCode.requestFocus()
            }
        }

        btnNavUp.apply {
            backgroundTintList = ColorStateList.valueOf(ThemePalette.purple)
            setTextColor(btnFg)
            setToolbarClickListener {
                val layout = etCode.layout
                if (layout != null) {
                    val pos = etCode.selectionStart
                    val line = layout.getLineForOffset(pos)
                    if (line > 0) {
                        val col = pos - layout.getLineStart(line)
                        val prevStart = layout.getLineStart(line - 1)
                        val prevEnd = layout.getLineEnd(line - 1)
                        val effEnd = if (prevEnd > prevStart && etCode.text[prevEnd - 1] == '\n') prevEnd - 1 else prevEnd
                        etCode.setSelection((prevStart + col).coerceAtMost(effEnd))
                    }
                }
                etCode.requestFocus()
            }
        }

        btnNavDown.apply {
            backgroundTintList = ColorStateList.valueOf(ThemePalette.purple)
            setTextColor(btnFg)
            setToolbarClickListener {
                val layout = etCode.layout
                if (layout != null) {
                    val pos = etCode.selectionStart
                    val line = layout.getLineForOffset(pos)
                    if (line < layout.lineCount - 1) {
                        val col = pos - layout.getLineStart(line)
                        val nextStart = layout.getLineStart(line + 1)
                        val nextEnd = layout.getLineEnd(line + 1)
                        val effEnd = if (nextEnd > nextStart && etCode.text[nextEnd - 1] == '\n') nextEnd - 1 else nextEnd
                        etCode.setSelection((nextStart + col).coerceAtMost(effEnd))
                    }
                }
                etCode.requestFocus()
            }
        }

        btnCancel.apply {
            backgroundTintList = ColorStateList.valueOf(ThemePalette.red)
            setTextColor(btnFg)
            setToolbarClickListener { dialog.dismiss() }
        }

        btnTab.apply {
            backgroundTintList = ColorStateList.valueOf(ThemePalette.yellow)
            setTextColor(btnFg)
            iconTint = ColorStateList.valueOf(btnFg)
            setToolbarClickListener {
                if (session.commandRunning) return@setToolbarClickListener
                selectAutocompleteEditor(etCode)
                val cursor = etCode.selectionStart.coerceAtLeast(0)
                val fullText = etCode.text.toString()
                var wordStart = cursor
                while (wordStart > 0 && (fullText[wordStart - 1].isLetterOrDigit() || fullText[wordStart - 1] == '_')) {
                    wordStart--
                }
                val prefix = fullText.substring(wordStart, cursor)
                if (prefix.isNotEmpty()) {
                    // Collect local variable names from script text (identifiers before '=')
                    val localVars = Regex("""^\s*([a-zA-Z_][\w]*)\s*=""", RegexOption.MULTILINE)
                        .findAll(fullText)
                        .map { it.groupValues[1] }
                        .filter { it.startsWith(prefix) && it != prefix }
                        .toList()
                    if (localVars.isNotEmpty()) {
                        val match = localVars.first()
                        etCode.text.replace(wordStart, cursor, match)
                        etCode.setSelection((wordStart + match.length).coerceAtMost(etCode.text.length))
                        return@setToolbarClickListener
                    } else {
                        val outCursor = intArrayOf(prefix.length)
                        val completed = session.engine.nativeAutocomplete(prefix, prefix.length, outCursor)
                        if (completed != prefix) {
                            etCode.text.replace(wordStart, cursor, completed)
                            etCode.setSelection((wordStart + completed.length).coerceAtMost(etCode.text.length))
                            return@setToolbarClickListener
                        }
                    }
                }
                // Default Tab action in code editor: indent with 4 spaces
                val selStart = etCode.selectionStart.coerceAtLeast(0)
                val selEnd = etCode.selectionEnd.coerceAtLeast(0)
                val minSel = minOf(selStart, selEnd)
                val maxSel = maxOf(selStart, selEnd)
                etCode.text.replace(minSel, maxSel, "    ")
                etCode.setSelection(minSel + 4)
            }
        }

        btnRun.apply {
            backgroundTintList = ColorStateList.valueOf(ThemePalette.yellow)
            setTextColor(btnFg)
            setToolbarClickListener {
                if (session.commandRunning) return@setToolbarClickListener
                val newName = etName.text.toString().trim()
                val newCode = etCode.text.toString().trim()
                if (newName.isNotEmpty() && newCode.isNotEmpty()) {
                    queueCommand(newCode, newName)
                } else {
                    Toast.makeText(activity, R.string.ui_text_018, Toast.LENGTH_SHORT).show()
                }
            }
        }

        btnSave.apply {
            backgroundTintList = ColorStateList.valueOf(btnAqua)
            iconTint = ColorStateList.valueOf(btnFg)
            setToolbarClickListener {
                if (session.commandRunning) return@setToolbarClickListener
                val newName = etName.text.toString().trim()
                val newCode = etCode.text.toString().trim()
                if (newName.isNotEmpty() && newCode.isNotEmpty()) {
                    if (onSave(newName, newCode)) dialog.dismiss()
                } else {
                    Toast.makeText(activity, R.string.ui_text_018, Toast.LENGTH_SHORT).show()
                }
            }
        }

        dialog.setOnDismissListener {
            draft.nameCursor = etName.selectionStart
            draft.codeCursor = etCode.selectionStart
            onDismiss(dialog)
        }
        onOpen(dialog)
        dialog.show()

        dialog.window?.apply {
            setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
            setSoftInputMode(android.view.WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE)
            setBackgroundDrawable(ColorDrawable(bgColor))
            if (android.os.Build.VERSION.SDK_INT < 35) statusBarColor = bgColor
            val controller = androidx.core.view.WindowCompat.getInsetsController(this, decorView)
            controller.isAppearanceLightStatusBars = androidx.core.graphics.ColorUtils.calculateLuminance(bgColor) > 0.5
            controller.isAppearanceLightNavigationBars = androidx.core.graphics.ColorUtils.calculateLuminance(bgColor) > 0.5
        }
    }
}
