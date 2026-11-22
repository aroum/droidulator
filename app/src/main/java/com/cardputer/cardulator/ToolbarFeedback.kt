package com.cardputer.cardulator

import android.content.Context
import android.view.HapticFeedbackConstants
import android.view.View

/** Shared by Activity and script-editor toolbars; dropdowns keep their normal listeners. */
internal fun View.setToolbarClickListener(action: (View) -> Unit) {
    setOnClickListener { view ->
        if (context.getSharedPreferences("droidulator_prefs", Context.MODE_PRIVATE)
                .getBoolean("toolbar_haptics", false)) {
            view.performHapticFeedback(HapticFeedbackConstants.KEYBOARD_TAP)
        }
        action(view)
    }
}
