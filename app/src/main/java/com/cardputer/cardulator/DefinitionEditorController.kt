package com.cardputer.cardulator

import android.app.Activity
import android.app.Dialog
import android.widget.EditText

internal class DefinitionEditorController(
    private val activity: Activity,
    private val session: CalculatorSession,
    private val dialogs: CalculatorDialogs,
    private val highlighter: CalculatorSyntaxHighlighter,
    private val submit: (String, DefinitionDraft) -> Boolean,
    private val untrack: (Dialog) -> Unit
) {
    data class Editor(val draft: DefinitionDraft, val dialog: Dialog, val input: EditText)
    var editor: Editor? = null
        private set

    fun show(title: String, initialExpr: String, hintText: String, tab: String) {
        val draft = session.definitionDraft ?: DefinitionDraft(title, hintText, tab, initialExpr)
            .also { session.definitionDraft = it }
        val et = dialogs.definitionInput(draft, hintText) { input, text ->
            if (!highlighter.isApplying(text)) highlighter.schedule(input)
        }

        if (initialExpr.isNotEmpty()) {
            et.post { highlighter.apply(et.editableText) }
        }

        val dialog = dialogs.show(title, et, positiveText = activity.getString(R.string.action_save), positiveValidator = {
            val valStr = et.text.toString().trim()
            if (valStr.isEmpty()) { et.error = activity.getString(R.string.ui_text_017); false }
            else {
                val expression = if (tab == "Constants" && !valStr.startsWith("const ")) "const $valStr" else valStr
                if (submit(expression, draft)) {
                    editor?.dialog?.setCancelable(false)
                    et.isEnabled = false
                }
                false
            }
        })
        editor = Editor(draft, dialog, et)
        dialog.setOnDismissListener {
            untrack(dialog)
            if (editor?.dialog === dialog) editor = null
            if (activity.isChangingConfigurations) draft.cursor = et.selectionStart
            else if (session.definitionDraft === draft) session.definitionDraft = null
        }
    }

}
