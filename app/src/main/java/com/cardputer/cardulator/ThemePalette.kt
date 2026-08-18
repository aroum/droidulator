package com.cardputer.cardulator

import android.graphics.Color
import org.json.JSONObject

/**
 * Centralized Theme Palette for Droidulator.
 * Supports light, dark, system auto modes, and custom JSON themes.
 */
object ThemePalette {
    var isDark: Boolean = true

    // Custom theme overrides (JSON parsed colors)
    private var customDarkColors: Map<String, Int>? = null
    private var customLightColors: Map<String, Int>? = null
    var isCustomThemeLoaded: Boolean = false

    fun loadCustomThemeJson(jsonString: String): Boolean {
        return try {
            val obj = JSONObject(jsonString)
            val darkMap = mutableMapOf<String, Int>()
            val lightMap = mutableMapOf<String, Int>()

            if (obj.has("dark")) {
                val darkObj = obj.getJSONObject("dark")
                parseModeObj(darkObj, darkMap)
            }
            if (obj.has("light")) {
                val lightObj = obj.getJSONObject("light")
                parseModeObj(lightObj, lightMap)
            }

            customDarkColors = darkMap
            customLightColors = lightMap
            cachedCustomRainbow = null
            isCustomThemeLoaded = darkMap.isNotEmpty() || lightMap.isNotEmpty()
            isCustomThemeLoaded
        } catch (e: Exception) {
            e.printStackTrace()
            false
        }
    }

    fun resetToBuiltIn() {
        customDarkColors = null
        customLightColors = null
        cachedCustomRainbow = null
        isCustomThemeLoaded = false
    }

    private fun parseModeObj(modeObj: JSONObject, targetMap: MutableMap<String, Int>) {
        modeObj.keys().forEach { key ->
            try {
                targetMap[key] = Color.parseColor(modeObj.getString(key))
            } catch (_: Exception) {}
        }
    }

    // Pre-parsed cached default colors for performance (zero allocation on property access)
    private val DEFAULT_DARK_BG = Color.parseColor("#282828")
    private val DEFAULT_LIGHT_BG = Color.parseColor("#FBF1C7")
    private val DEFAULT_DARK_BG_SOFT = Color.parseColor("#3C3836")
    private val DEFAULT_LIGHT_BG_SOFT = Color.parseColor("#EBDBB2")
    private val DEFAULT_DARK_BG_DARK_TEXT = Color.parseColor("#282828")
    private val DEFAULT_LIGHT_BG_DARK_TEXT = Color.parseColor("#282828")
    private val DEFAULT_DARK_FG = Color.parseColor("#EBDBB2")
    private val DEFAULT_LIGHT_FG = Color.parseColor("#3C3836")
    private val DEFAULT_DARK_FG_DIM = Color.parseColor("#A89984")
    private val DEFAULT_LIGHT_FG_DIM = Color.parseColor("#7C6F64")
    private val DEFAULT_DARK_BORDER = Color.parseColor("#504945")
    private val DEFAULT_LIGHT_BORDER = Color.parseColor("#A89984")
    private val DEFAULT_DARK_AQUA = Color.parseColor("#689D6A")
    private val DEFAULT_LIGHT_AQUA = Color.parseColor("#427B58")
    private val DEFAULT_DARK_GREEN = Color.parseColor("#8EC07C")
    private val DEFAULT_LIGHT_GREEN = Color.parseColor("#98971A")
    private val DEFAULT_DARK_YELLOW = Color.parseColor("#FABD2F")
    private val DEFAULT_LIGHT_YELLOW = Color.parseColor("#B57614")
    private val DEFAULT_DARK_YELLOW_DARK = Color.parseColor("#D79921")
    private val DEFAULT_LIGHT_YELLOW_DARK = Color.parseColor("#B57614")
    private val DEFAULT_DARK_ORANGE = Color.parseColor("#FE8019")
    private val DEFAULT_LIGHT_ORANGE = Color.parseColor("#AF3A03")
    private val DEFAULT_DARK_ORANGE_DARK = Color.parseColor("#D65D0E")
    private val DEFAULT_LIGHT_ORANGE_DARK = Color.parseColor("#AF3A03")
    private val DEFAULT_DARK_RED = Color.parseColor("#FB4934")
    private val DEFAULT_LIGHT_RED = Color.parseColor("#9D0006")
    private val DEFAULT_DARK_PURPLE = Color.parseColor("#D3869B")
    private val DEFAULT_LIGHT_PURPLE = Color.parseColor("#8F3F71")
    private val DEFAULT_DARK_BLUE = Color.parseColor("#83A598")
    private val DEFAULT_LIGHT_BLUE = Color.parseColor("#076678")

    private val DEFAULT_DARK_RAINBOW = listOf(
        DEFAULT_DARK_YELLOW, DEFAULT_DARK_ORANGE,
        DEFAULT_DARK_RED, DEFAULT_DARK_PURPLE,
        DEFAULT_DARK_BLUE, DEFAULT_DARK_GREEN,
        Color.parseColor("#B8BB26")
    )
    private val DEFAULT_LIGHT_RAINBOW = listOf(
        DEFAULT_LIGHT_YELLOW, DEFAULT_LIGHT_ORANGE,
        DEFAULT_LIGHT_RED, DEFAULT_LIGHT_PURPLE,
        DEFAULT_LIGHT_BLUE, DEFAULT_LIGHT_AQUA,
        Color.parseColor("#79740E")
    )

    private var cachedCustomRainbow: List<Int>? = null

    private inline fun getColor(key: String, defaultDark: Int, defaultLight: Int): Int {
        val map = if (isDark) customDarkColors else customLightColors
        if (map != null) {
            val color = map[key]
            if (color != null) return color
        }
        return if (isDark) defaultDark else defaultLight
    }

    // Backgrounds & Surface Colors
    val bg: Int get() = getColor("bg", DEFAULT_DARK_BG, DEFAULT_LIGHT_BG)
    val bgSoft: Int get() = getColor("bg_soft", DEFAULT_DARK_BG_SOFT, DEFAULT_LIGHT_BG_SOFT)
    val bgCard: Int get() = getColor("bg_card", DEFAULT_DARK_BG_SOFT, DEFAULT_LIGHT_BG_SOFT)
    val bgDarkText: Int get() = getColor("bg_dark_text", DEFAULT_DARK_BG_DARK_TEXT, DEFAULT_LIGHT_BG_DARK_TEXT)

    // Foreground Text & Dim Colors
    val fgText: Int get() = getColor("fg_text", DEFAULT_DARK_FG, DEFAULT_LIGHT_FG)
    val fgDim: Int get() = getColor("fg_dim", DEFAULT_DARK_FG_DIM, DEFAULT_LIGHT_FG_DIM)
    val fgInverted: Int get() = getColor("fg_inverted", DEFAULT_DARK_BG, DEFAULT_LIGHT_BG)
    val borderColor: Int get() = getColor("border_color", DEFAULT_DARK_BORDER, DEFAULT_LIGHT_BORDER)

    // Brand & Accent Colors
    val aqua: Int get() = getColor("aqua", DEFAULT_DARK_AQUA, DEFAULT_LIGHT_AQUA)
    val green: Int get() = getColor("green", DEFAULT_DARK_GREEN, DEFAULT_LIGHT_GREEN)
    val yellow: Int get() = getColor("yellow", DEFAULT_DARK_YELLOW, DEFAULT_LIGHT_YELLOW)
    val yellowDark: Int get() = getColor("yellow_dark", DEFAULT_DARK_YELLOW_DARK, DEFAULT_LIGHT_YELLOW_DARK)
    val orange: Int get() = getColor("orange", DEFAULT_DARK_ORANGE, DEFAULT_LIGHT_ORANGE)
    val orangeDark: Int get() = getColor("orange_dark", DEFAULT_DARK_ORANGE_DARK, DEFAULT_LIGHT_ORANGE_DARK)
    val red: Int get() = getColor("red", DEFAULT_DARK_RED, DEFAULT_LIGHT_RED)
    val purple: Int get() = getColor("purple", DEFAULT_DARK_PURPLE, DEFAULT_LIGHT_PURPLE)
    val blue: Int get() = getColor("blue", DEFAULT_DARK_BLUE, DEFAULT_LIGHT_BLUE)

    // Rainbow Brackets Palette (7 stages) — Zero allocation for built-in themes
    val rainbowColors: List<Int>
        get() {
            if (!isCustomThemeLoaded) {
                return if (isDark) DEFAULT_DARK_RAINBOW else DEFAULT_LIGHT_RAINBOW
            }
            val map = if (isDark) customDarkColors else customLightColors
            if (map != null && map.containsKey("rainbow_0")) {
                cachedCustomRainbow?.let { return it }
                val list = (0..6).mapNotNull { map["rainbow_$it"] }
                if (list.isNotEmpty()) {
                    cachedCustomRainbow = list
                    return list
                }
            }
            return if (isDark) DEFAULT_DARK_RAINBOW else DEFAULT_LIGHT_RAINBOW
        }
}
