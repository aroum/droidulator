package com.cardputer.cardulator

import android.content.Context
import android.content.res.Configuration
import android.net.Uri
import java.io.ByteArrayOutputStream
import java.io.FileNotFoundException

internal class CalculatorThemeSettings(private val context: Context) {
    private val preferences = context.getSharedPreferences("droidulator_prefs", Context.MODE_PRIVATE)

    var mode: Int
        get() = preferences.getInt("theme_mode", 0)
        set(value) { preferences.edit().putInt("theme_mode", value).apply() }

    fun isLight(configuration: Configuration, mode: Int): Boolean = when (mode) {
        1 -> false
        2 -> true
        else -> (configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) != Configuration.UI_MODE_NIGHT_YES
    }

    fun restoreCustomPalette() {
        preferences.getString("custom_theme_json", null)?.let(ThemePalette::loadCustomThemeJson)
    }

    fun resetCustomPalette() {
        ThemePalette.resetToBuiltIn()
        preferences.edit().remove("custom_theme_json").apply()
    }

    fun importCustomPalette(uri: Uri): Boolean {
        val stream = context.contentResolver.openInputStream(uri) ?: throw FileNotFoundException()
        val json = stream.use { input ->
            val buffer = ByteArray(8192)
            val output = ByteArrayOutputStream()
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                require(output.size() + count <= 1024 * 1024) { "Theme file exceeds 1 MiB" }
                output.write(buffer, 0, count)
            }
            output.toString("UTF-8")
        }
        if (!ThemePalette.loadCustomThemeJson(json)) return false
        preferences.edit().putString("custom_theme_json", json).apply()
        return true
    }
}
