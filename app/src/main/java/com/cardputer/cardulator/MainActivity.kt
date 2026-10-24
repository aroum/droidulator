package com.cardputer.cardulator

import android.app.Dialog
import android.content.res.ColorStateList
import android.content.res.Configuration
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.text.Editable
import android.text.Spannable
import android.text.TextWatcher
import android.text.style.ForegroundColorSpan
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {

    private lateinit var mainRoot: LinearLayout
    private lateinit var topHeaderBar: LinearLayout
    private lateinit var spinnerTabs: Spinner
    private lateinit var tvMode: TextView
    private lateinit var btnRunScript: Button
    private lateinit var btnAddItem: Button

    private lateinit var tvOutput: TextView
    private lateinit var scrollView: ScrollView
    private lateinit var scrollSettings: ScrollView
    private lateinit var scrollItems: ScrollView
    private lateinit var itemsContainer: LinearLayout
    private lateinit var plotView: PlotView

    private lateinit var etInput: EditText
    private lateinit var tvErrorOverlay: TextView

    private lateinit var toolbarKeyboard: LinearLayout
    private lateinit var btnNavLeft: Button
    private lateinit var btnNavRight: Button
    private lateinit var btnNavUp: Button
    private lateinit var btnNavDown: Button
    private lateinit var btnTab: Button
    private lateinit var btnClear: Button

    // Settings UI elements
    private lateinit var rgThemeMode: RadioGroup
    private lateinit var rbThemeAuto: RadioButton
    private lateinit var rbThemeDark: RadioButton
    private lateinit var rbThemeLight: RadioButton
    private lateinit var btnLoadCustomTheme: Button
    private lateinit var btnResetCustomTheme: Button
    private lateinit var btnSettingsEnterAction: Button
    private lateinit var btnSettingsClearHistory: Button
    private lateinit var cbDigitGrouping: CheckBox
    private lateinit var cbHideWelcome: CheckBox
    private lateinit var cbAutoPair: CheckBox
    private lateinit var tvSettingsSectionTheme: TextView
    private lateinit var tvSettingsSectionCustomTheme: TextView
    private lateinit var tvSettingsSectionDisplay: TextView
    private lateinit var tvSettingsSectionEnterAction: TextView

    // Number Formatting Spinners
    private lateinit var spinnerFmtMode: Spinner
    private lateinit var spinnerFmtPrec: Spinner
    private val fmtModeItems = arrayOf("NORM", "FLT", "FIX", "SCI", "ENG")
    private val fmtModeItemsDisplay = arrayOf("NORM ▼", "FLT ▼", "FIX ▼", "SCI ▼", "ENG ▼")
    private val fmtPrecItems = arrayOf("0dp", "1dp", "2dp", "3dp", "4dp", "5dp", "6dp", "7dp", "8dp", "9dp", "10dp", "11dp", "12dp")
    private val fmtPrecItemsDisplay = arrayOf("0dp ▼", "1dp ▼", "2dp ▼", "3dp ▼", "4dp ▼", "5dp ▼", "6dp ▼", "7dp ▼", "8dp ▼", "9dp ▼", "10dp ▼", "11dp ▼", "12dp ▼")

    private var fmtMode = 0
    private var fmtPrecision = 6
    private var isDigitGroupingActive = false
    private var hideWelcomeMessage = false
    private var isAutoPairEnabled = true

    // Enter Key Action: true = Send command, false = New line
    private var isEnterSendMode = true

    private var currentTabPosition = 0
    private val tabs = arrayOf("REPL", "Scripts", "Formulas", "Constants", "Variables", "Plot", "Settings")
    private val tabsDisplay = arrayOf("REPL ▼", "Scripts ▼", "Formulas ▼", "Constants ▼", "Variables ▼", "Plot ▼", "Settings ▼")

    // Built-in scripts storage for Scripts menu tab
    private val userScripts = mutableListOf(
        Pair("fibonacci", "x = 1\ny = 1\nfor (i = 1; i <= 10; i++) {\n  print(x)\n  tmp = x + y\n  x = y\n  y = tmp\n}"),
        Pair("factorial_test", "n = 5\nres = 1\nfor (i = 1; i <= n; i++) {\n  res = res * i\n}\nprint(res)")
    )

    // Theme Mode: 0 = Auto (System), 1 = Dark, 2 = Light
    private var themeMode = 0
    private var isLightThemeActive = false
    private lateinit var btnDeleteAll: Button
    private lateinit var btnToggleEVars: Button

    private var showEVars = false

    // REPL History management
    private val replHistory = mutableListOf<String>()
    private var replHistoryIndex = -1
    private var tempReplInput = ""

    // Flag to avoid infinite loop in TextWatcher during syntax highlighting
    private var isHighlighting = false

    // Cycling Tab Autocomplete state for conv and plot contexts
    private var convTabOrigPrefix: String? = null
    private var convTabSplitPos: Int = -1
    private var convTabMatchIdx: Int = 0

    companion object {
        private const val WELCOME_MSG = "Welcome to Droidulator Math REPL\nType expressions like '2+3*4' or 'sin(90)'\n"

        // Pre-compiled regex patterns for zero-allocation syntax highlighting
        private val NUMBER_REGEX = Regex("\\b\\d+(\\.\\d+)?\\b")
        private val KEYWORD_REGEX = Regex("\\b(for|while|if|else|return|const)\\b")
        private val FUNC_REGEX = Regex("\\b(sin|cos|tan|ctan|asin|acos|atan|sinh|cosh|tanh|sqrt|ln|log|exp|cbrt|abs|ceil|floor|round|trunc|sgn|mod|mean|median|var|std|rUni|rNor|fact|gcd|lcm|fib|len|print|run|plot|conv)\\b")
        private val CONST_REGEX = Regex("\\b(pi|e\\d+)\\b|(?<![a-zA-Z0-9_])e(?![a-zA-Z0-9_])")
        private val OP_REGEX = Regex("[+\\-*/^=><!%]")
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Initialize C++ Engine
        CardulatorEngine.nativeInit()
        loadUserDataFromStorage()

        // Extract units.dat from assets to internal storage so the C bridge can read it
        val unitsDatFile = java.io.File(filesDir, "units.dat")
        if (!unitsDatFile.exists()) {
            assets.open("units.dat").use { input ->
                unitsDatFile.outputStream().use { output -> input.copyTo(output) }
            }
        }
        CardulatorEngine.nativeInitUnits(unitsDatFile.absolutePath)

        // Bind Views
        mainRoot = findViewById(R.id.main_root)
        topHeaderBar = mainRoot.getChildAt(0) as LinearLayout
        spinnerTabs = findViewById(R.id.spinner_tabs)
        tvMode = findViewById(R.id.tv_mode)
        btnRunScript = findViewById(R.id.btn_run_script)
        btnAddItem = findViewById(R.id.btn_add_item)
        btnDeleteAll = findViewById(R.id.btn_delete_all)
        btnToggleEVars = findViewById(R.id.btn_toggle_evars)

        tvOutput = findViewById(R.id.tv_output)
        scrollView = findViewById(R.id.scroll_view)
        scrollSettings = findViewById(R.id.scroll_settings)
        scrollItems = findViewById(R.id.scroll_items)
        itemsContainer = findViewById(R.id.items_container)
        plotView = findViewById(R.id.plot_view)

        etInput = findViewById(R.id.et_input)
        tvErrorOverlay = findViewById(R.id.tv_error_overlay)

        toolbarKeyboard = findViewById(R.id.toolbar_keyboard)
        btnNavLeft = findViewById(R.id.btn_nav_left)
        btnNavRight = findViewById(R.id.btn_nav_right)
        btnNavUp = findViewById(R.id.btn_nav_up)
        btnNavDown = findViewById(R.id.btn_nav_down)
        btnTab = findViewById(R.id.btn_tab)
        btnClear = findViewById(R.id.btn_clear)

        // Settings views
        rgThemeMode = findViewById(R.id.rg_theme_mode)
        rbThemeAuto = findViewById(R.id.rb_theme_auto)
        rbThemeDark = findViewById(R.id.rb_theme_dark)
        rbThemeLight = findViewById(R.id.rb_theme_light)
        btnLoadCustomTheme = findViewById(R.id.btn_load_custom_theme)
        btnResetCustomTheme = findViewById(R.id.btn_reset_custom_theme)
        btnSettingsEnterAction = findViewById(R.id.btn_settings_enter_action)
        btnSettingsClearHistory = findViewById(R.id.btn_settings_clear_history)
        cbDigitGrouping = findViewById(R.id.cb_digit_grouping)
        cbHideWelcome = findViewById(R.id.cb_hide_welcome)
        cbAutoPair = findViewById(R.id.cb_auto_pair)
        tvSettingsSectionTheme = findViewById(R.id.tv_settings_section_theme)
        tvSettingsSectionCustomTheme = findViewById(R.id.tv_settings_section_custom_theme)
        tvSettingsSectionDisplay = findViewById(R.id.tv_settings_section_display)
        tvSettingsSectionEnterAction = findViewById(R.id.tv_settings_section_enter_action)

        // Number Formatting Spinners
        spinnerFmtMode = findViewById(R.id.spinner_fmt_mode)
        spinnerFmtPrec = findViewById(R.id.spinner_fmt_prec)

        // Load saved formatting & theme preferences
        val prefs = getSharedPreferences("droidulator_prefs", MODE_PRIVATE)
        fmtMode = prefs.getInt("fmt_mode", 0)
        fmtPrecision = prefs.getInt("fmt_precision", 6)
        isDigitGroupingActive = prefs.getBoolean("digit_grouping", false)
        hideWelcomeMessage = prefs.getBoolean("hide_welcome", false)
        isAutoPairEnabled = prefs.getBoolean("auto_pair", true)
        showEVars = prefs.getBoolean("show_evars", false)
        themeMode = prefs.getInt("theme_mode", 0)

        cbHideWelcome.isChecked = hideWelcomeMessage
        cbAutoPair.isChecked = isAutoPairEnabled
        if (hideWelcomeMessage) {
            tvOutput.text = ""
        } else {
            tvOutput.text = WELCOME_MSG
        }

        listOf(rbThemeAuto, rbThemeDark, rbThemeLight).getOrNull(themeMode)?.isChecked = true
        if (themeMode !in 0..2) rbThemeAuto.isChecked = true

        val savedThemeJson = prefs.getString("custom_theme_json", null)
        if (!savedThemeJson.isNullOrEmpty()) {
            ThemePalette.loadCustomThemeJson(savedThemeJson)
        }

        CardulatorEngine.nativeSetFormatSettings(fmtMode, fmtPrecision, isDigitGroupingActive)

        setupTabSpinner()
        setupFormatSpinners()
        setupInputHandlers()
        setupControlButtons()
        setupNavigationButtons()
        setupErrorOverlayHandlers()
        setupSettingsHandlers()

        // Apply theme upon creation
        applyTheme()
    }

    private fun setupTabSpinner() {
        val adapter = object : ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, tabsDisplay) {
            override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
                val v = super.getView(position, convertView, parent) as TextView
                v.text = tabsDisplay[position]
                v.setTextColor(ThemePalette.green)
                v.setTypeface(android.graphics.Typeface.MONOSPACE, android.graphics.Typeface.BOLD)
                v.textSize = 14f
                v.gravity = android.view.Gravity.START or android.view.Gravity.CENTER_VERTICAL
                v.setPadding(0, 0, 0, 0)
                return v
            }

            override fun getDropDownView(position: Int, convertView: View?, parent: ViewGroup): View {
                val v = super.getDropDownView(position, convertView, parent) as TextView
                v.text = tabs[position]
                v.setBackgroundColor(ThemePalette.bgSoft)
                v.setTextColor(ThemePalette.fgText)
                v.setTypeface(android.graphics.Typeface.MONOSPACE, android.graphics.Typeface.BOLD)
                v.textSize = 14f
                v.setPadding(16, 16, 16, 16)
                return v
            }
        }
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinnerTabs.adapter = adapter

        spinnerTabs.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                currentTabPosition = position
                val selectedTab = tabs[position]
                refreshTabContent(selectedTab)
            }

            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
    }

    private fun setupFormatSpinners() {
        fun createCustomAdapter(displayItems: Array<String>, rawItems: Array<String>, colorRes: () -> Int): ArrayAdapter<String> {
            val adapter = object : ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, displayItems) {
                override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
                    val v = super.getView(position, convertView, parent) as TextView
                    v.text = displayItems[position]
                    v.setTextColor(colorRes())
                    v.setTypeface(android.graphics.Typeface.MONOSPACE, android.graphics.Typeface.BOLD)
                    v.textSize = 14f
                    v.gravity = android.view.Gravity.END or android.view.Gravity.CENTER_VERTICAL
                    v.setPadding(0, 0, 0, 0)
                    return v
                }

                override fun getDropDownView(position: Int, convertView: View?, parent: ViewGroup): View {
                    val v = super.getDropDownView(position, convertView, parent) as TextView
                    v.text = rawItems[position]
                    v.setBackgroundColor(ThemePalette.bgSoft)
                    v.setTextColor(ThemePalette.fgText)
                    v.setTypeface(android.graphics.Typeface.MONOSPACE, android.graphics.Typeface.BOLD)
                    v.textSize = 14f
                    v.setPadding(16, 16, 16, 16)
                    return v
                }
            }
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
            return adapter
        }

        spinnerFmtMode.adapter = createCustomAdapter(fmtModeItemsDisplay, fmtModeItems) { ThemePalette.orange }
        spinnerFmtPrec.adapter = createCustomAdapter(fmtPrecItemsDisplay, fmtPrecItems) { ThemePalette.yellow }

        spinnerFmtMode.setSelection(fmtMode.coerceIn(0, fmtModeItems.size - 1))
        spinnerFmtPrec.setSelection(fmtPrecision.coerceIn(0, fmtPrecItems.size - 1))

        var modeUserInit = false
        spinnerFmtMode.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                if (!modeUserInit) { modeUserInit = true; return }
                if (fmtMode != position) {
                    fmtMode = position
                    updateFormatSettings()
                }
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }

        var precUserInit = false
        spinnerFmtPrec.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                if (!precUserInit) { precUserInit = true; return }
                if (fmtPrecision != position) {
                    fmtPrecision = position
                    updateFormatSettings()
                }
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
    }

    private fun setupInputHandlers() {
        // Multi-line input behavior configuration
        etInput.setOnEditorActionListener { _, actionId, event ->
            if (tabs[currentTabPosition] == "REPL" && isEnterSendMode) {
                if (actionId == EditorInfo.IME_ACTION_DONE || actionId == EditorInfo.IME_ACTION_SEND || actionId == EditorInfo.IME_NULL ||
                    (event != null && event.keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_DOWN)) {
                    submitCurrentInput()
                    true
                } else false
            } else false
        }

        // Intercept Enter key on key listener for multiline input field
        etInput.setOnKeyListener { _, keyCode, event ->
            if (keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_DOWN) {
                if (tabs[currentTabPosition] == "REPL" && isEnterSendMode) {
                    submitCurrentInput()
                    true
                } else false
            } else false
        }

        // Syntax highlighting, bracket pairing, auto-brackets & auto-quotes
        etInput.addTextChangedListener(object : TextWatcher {
            private var beforeText = ""

            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {
                beforeText = s?.toString() ?: ""
            }

            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
                if (isHighlighting || s == null) return

                val currentText = s.toString()
                // Intercept inserted newline in REPL mode when enter send mode is active
                if (tabs[currentTabPosition] == "REPL" && isEnterSendMode && currentText.contains("\n")) {
                    isHighlighting = true
                    // Replace newlines with spaces so multiline expressions/scripts don't collapse words together
                    val cleanText = currentText.replace("\r\n", " ").replace("\n", " ").replace(Regex("""\s+"""), " ")
                    etInput.setText(cleanText)
                    etInput.setSelection(cleanText.length)
                    isHighlighting = false
                    submitCurrentInput()
                    return
                }

                // Auto-brackets and auto-quotes insertion when typing a single character
                if (isAutoPairEnabled && count == 1 && before == 0 && start < currentText.length) {
                    val typedChar = currentText[start]
                    val insertedPair = when (typedChar) {
                        '(' -> ")"
                        '[' -> "]"
                        '{' -> "}"
                        '"' -> "\""
                        '\'' -> "'"
                        else -> null
                    }
                    if (insertedPair != null) {
                        isHighlighting = true
                        val newStr = currentText.substring(0, start + 1) + insertedPair + currentText.substring(start + 1)
                        etInput.setText(newStr)
                        etInput.setSelection(start + 1)
                        isHighlighting = false
                    }
                }
            }

            override fun afterTextChanged(s: Editable?) {
                if (isHighlighting || s == null) return
                applySyntaxHighlighting(s)
            }
        })
    }

    private fun setupControlButtons() {
        // Tab Autocomplete button & cursor insertion inside brackets
        btnTab.setOnClickListener {
            val fullText = etInput.text.toString()
            val caret = etInput.selectionStart.coerceAtLeast(0)

            // Detect conv( context: suggest units/resistor codes inside conv(...)
            val convMatch = Regex("""conv\(([^)]*)$""").find(fullText.substring(0, caret))
            if (convMatch != null) {
                val lastCommaPos = fullText.lastIndexOf(',', caret - 1)
                val lastParenPos = fullText.lastIndexOf('(', caret - 1)
                val splitPos = if (lastCommaPos > lastParenPos) lastCommaPos + 1 else lastParenPos + 1

                val argsContent = convMatch.groupValues[1]
                val parts = argsContent.split(",")
                val lastPartRaw = parts.last().trim()
                val lastPart = lastPartRaw.trim('"', '\'')

                // Detect source unit (fromUnit) if present in preceding arguments
                val fromUnit: String? = when {
                    parts.size >= 3 -> {
                        val prevArg = parts[parts.size - 2].trim().trim('"', '\'')
                        if (prevArg.isNotEmpty()) prevArg else null
                    }
                    parts.size == 2 -> {
                        val firstArg = parts[0].trim().trim('"', '\'')
                        // Check if first arg contains a number followed by a unit (e.g. "10 mm", "10mm", "100 ohm")
                        val m = Regex("""^[\d.\s\+\-\*\/\^eE]+\s*([a-zA-Z][a-zA-Z\^\d_]*)$""").find(firstArg)
                        m?.groupValues?.get(1)?.trim()
                    }
                    else -> null
                }

                val unitCandidates = listOf(
                    // Length & Distance
                    "mm", "mmHg", "mm^2", "mm2", "std_mm2", "m", "meter", "cm", "km", "mile", "yard", "foot", "inch", "in", "ft", "yd", "nauticalmile", "angstrom", "au",
                    // Area & Volume
                    "m^2", "m2", "cm^2", "cm2", "km^2", "km2", "hectare", "acre", "sqft", "sqin",
                    "m^3", "m3", "liter", "l", "ml", "dl", "cl", "gal", "gallon", "quart", "pint", "cup", "floz", "tbsp", "tsp", "barrel",
                    // Pressure
                    "torr", "atm", "bar", "mbar", "Pa", "pascal", "kPa", "MPa", "GPa", "psi", "psf", "inHg",
                    // Electronics & Component Codes
                    "ohm", "mohm", "kohm", "Mohm", "smd3", "smd4", "eia96", "awg", "std_awg",
                    "farad", "F", "uF", "uf", "nF", "nf", "pF", "pf",
                    "henry", "H", "uH", "uh", "mH", "mh", "nH", "nh",
                    "ampere", "amp", "A", "mA", "uA", "kA",
                    "volt", "V", "mV", "uV", "kV", "MV",
                    "watt", "W", "mW", "uW", "kW", "MW", "GW", "hp", "horsepower",
                    "Hz", "khz", "mhz", "ghz", "coulomb", "tesla", "weber",
                    // Decibels & Ratios
                    "dB", "db", "dBm", "dbm", "dBW", "dbw", "dBV", "dbv", "dBmV", "dbmv", "dBuV", "dbuv", "dBu", "dbu", "times", "ratio",
                    // Mass & Weight
                    "kg", "kilogram", "g", "gram", "mg", "ug", "ton", "tonne", "lb", "pound", "oz", "ounce", "stone", "carat",
                    // Time
                    "s", "sec", "second", "ms", "us", "ns", "min", "minute", "hr", "hour", "day", "week", "month", "year",
                    // Speed
                    "m/s", "km/hr", "km/h", "kph", "mph", "knot", "mach",
                    // Acceleration
                    "m/s^2", "m/s2", "cm/s^2", "cm/s2", "km/s^2", "km/s2", "km/h^2", "km/h2", "ft/s^2", "ft/s2", "in/s^2", "in/s2", "gal", "g0", "gee", "gravity",
                    // Energy & Work
                    "joule", "J", "kJ", "MJ", "cal", "calorie", "kcal", "eV", "keV", "MeV", "GeV", "Wh", "kWh", "MWh", "BTU",
                    // Force
                    "newton", "N", "kN", "dyn", "dyne", "lbf", "kgf",
                    // Temperature & Angle
                    "degC", "degF", "kelvin", "K", "deg", "degree", "rad", "radian"
                )

                val fromDims = fromUnit?.let { getUnitDimensions(it) } ?: emptySet()
                val filteredCandidates = if (fromDims.isNotEmpty()) {
                    unitCandidates.filter { candidate ->
                        val cDims = getUnitDimensions(candidate)
                        cDims.any { it in fromDims }
                    }
                } else {
                    unitCandidates
                }

                val isContinuation = (convTabOrigPrefix != null && convTabSplitPos == splitPos)
                val origPrefix = if (!isContinuation) {
                    convTabOrigPrefix = lastPart
                    convTabSplitPos = splitPos
                    convTabMatchIdx = 0
                    lastPart
                } else {
                    convTabMatchIdx++
                    convTabOrigPrefix ?: lastPart
                }

                val matches = if (origPrefix.isEmpty()) {
                    filteredCandidates
                } else {
                    filteredCandidates.filter { it.startsWith(origPrefix, ignoreCase = true) }
                }

                if (matches.isNotEmpty()) {
                    val match = matches[convTabMatchIdx % matches.size]

                    val prefix = fullText.substring(0, splitPos).trimEnd()
                    var endArgPos = fullText.indexOfAny(charArrayOf(',', ')'), caret)
                    if (endArgPos == -1) {
                        endArgPos = fullText.length
                    }
                    val suffix = fullText.substring(endArgPos)

                    val formattedPrefix = if (prefix.endsWith("(") || prefix.endsWith(",")) "$prefix " else prefix
                    val newArg = "\"$match\""
                    val newText = "$formattedPrefix$newArg$suffix"

                    etInput.setText(newText)
                    etInput.setSelection(formattedPrefix.length + newArg.length)
                    return@setOnClickListener
                } else {
                    convTabOrigPrefix = null
                    convTabSplitPos = -1
                    convTabMatchIdx = 0
                }
            } else {
                convTabOrigPrefix = null
                convTabSplitPos = -1
                convTabMatchIdx = 0
            }

            // Detect plot( context: suggest plot parameters/functions inside plot(...)
            val plotMatch = Regex("""plot\(([^)]*)$""").find(fullText.substring(0, caret))
            if (plotMatch != null) {
                val argsContent = plotMatch.groupValues[1]
                val parts = argsContent.split(",")
                val lastPart = parts.last().trim().trim('"', '\'')
                val plotCandidates = listOf(
                    "sin(x)", "cos(x)", "tan(x)", "exp(x)", "log(x)", "sqrt(x)",
                    "x^2", "x^3", "abs(x)", "xlim", "ylim", "title", "grid"
                )
                val match = if (lastPart.isEmpty()) {
                    "sin(x)"
                } else {
                    plotCandidates.firstOrNull { it.startsWith(lastPart, ignoreCase = true) && !it.equals(lastPart, ignoreCase = true) }
                        ?: plotCandidates.firstOrNull { it.startsWith(lastPart, ignoreCase = true) }
                }
                if (match != null) {
                    val lastCommaPos = fullText.lastIndexOf(',', caret - 1)
                    val lastParenPos = fullText.lastIndexOf('(', caret - 1)
                    val splitPos = if (lastCommaPos > lastParenPos) lastCommaPos + 1 else lastParenPos + 1
                    val prefix = fullText.substring(0, splitPos)
                    val suffix = fullText.substring(caret)
                    val newText = prefix + match + suffix
                    etInput.setText(newText)
                    etInput.setSelection(prefix.length + match.length)
                    return@setOnClickListener
                }
            }

            // Detect run( context: complete script name from userScripts
            val runMatch = Regex("""run\("?([\w]*)"?\)?$""").find(fullText.substring(0, caret))
            if (runMatch != null) {
                val partial = runMatch.groupValues[1]
                val match = userScripts.map { it.first }.firstOrNull { it.startsWith(partial, ignoreCase = true) }
                if (match != null) {
                    val runCallStart = fullText.lastIndexOf("run(", caret - 1)
                    if (runCallStart != -1) {
                        val baseText = fullText.substring(0, runCallStart)
                        val replacement = "run(\"$match\")"
                        val newText = baseText + replacement
                        etInput.setText(newText)
                        etInput.setSelection(newText.length)
                        return@setOnClickListener
                    }
                }
            }

            // General autocomplete at caret position (preserves any text after caret e.g. closing bracket ')')
            val outCursor = intArrayOf(caret)
            val completedFull = CardulatorEngine.nativeAutocomplete(fullText, caret, outCursor)
            if (completedFull.isNotEmpty() && completedFull != fullText) {
                etInput.setText(completedFull)
                val newCursorPos = outCursor[0].coerceIn(0, completedFull.length)
                etInput.setSelection(newCursorPos.coerceIn(0, etInput.text.length))
            }
        }

        // Clear button (Short click: clear input field; 1000ms long press: clear all user variables)
        val clearHandler = android.os.Handler(android.os.Looper.getMainLooper())
        var isLongPressTriggered = false
        val longPressRunnable = Runnable {
            isLongPressTriggered = true
            val varsRaw = CardulatorEngine.nativeGetVariables()
            val lines = varsRaw.split("\n").filter { it.contains("=") }
            for (line in lines) {
                val varName = line.split("=")[0].trim()
                CardulatorEngine.nativeDeleteVariable(varName)
            }
            saveUserDataToStorage()
            Toast.makeText(this, "All variables deleted", Toast.LENGTH_SHORT).show()
            if (tabs[currentTabPosition] == "Variables") {
                refreshTabContent("Variables")
            }
        }

        btnClear.setOnTouchListener { _, event ->
            when (event.action) {
                android.view.MotionEvent.ACTION_DOWN -> {
                    isLongPressTriggered = false
                    clearHandler.postDelayed(longPressRunnable, 1000)
                }
                android.view.MotionEvent.ACTION_UP -> {
                    clearHandler.removeCallbacks(longPressRunnable)
                    if (!isLongPressTriggered) {
                        etInput.setText("")
                        dismissErrorOverlay()
                    }
                }
                android.view.MotionEvent.ACTION_CANCEL -> {
                    clearHandler.removeCallbacks(longPressRunnable)
                }
            }
            true
        }

        // Mode toggle (DEG / RAD)
        tvMode.setOnClickListener {
            toggleDegRad()
        }

        // Run Script button
        btnRunScript.setOnClickListener {
            val code = etInput.text.toString().trim()
            if (code.isNotEmpty()) {
                val log = CardulatorEngine.nativeRunScript(code)
                appendRawOutput("> SCRIPT OUTPUT:\n$log\n")
                showScriptOutputDialog("Script Output", log)
            } else {
                Toast.makeText(this, "Script is empty", Toast.LENGTH_SHORT).show()
            }
        }

        // Add Item button (Var / Const / Script)
        btnAddItem.setOnClickListener {
            showAddItemDialog()
        }

        // Delete All Items button (Red ✕)
        btnDeleteAll.setOnClickListener {
            val activeTab = tabs[currentTabPosition]
            val fgColor = ThemePalette.fgText
            val tvMsg = TextView(this).apply {
                text = "Are you sure you want to delete all $activeTab?"
                setTextColor(fgColor)
                textSize = 14f
                typeface = android.graphics.Typeface.MONOSPACE
            }
            showGruvboxCustomDialog("Delete All $activeTab", tvMsg, positiveText = "DELETE ALL") {
                when (activeTab) {
                    "Variables" -> {
                        val varsRaw = CardulatorEngine.nativeGetVariables()
                        val lines = varsRaw.split("\n").filter { it.contains("=") }
                        for (line in lines) {
                            val varName = line.split("=")[0].trim()
                            CardulatorEngine.nativeDeleteVariable(varName)
                        }
                    }
                    "Formulas" -> {
                        val formulasRaw = CardulatorEngine.nativeGetFormulas()
                        val lines = formulasRaw.split("\n").filter { it.contains("=") || it.contains("(") }
                        for (line in lines) {
                            val fnName = line.split("=")[0].split("(")[0].trim()
                            CardulatorEngine.nativeDeleteFormula(fnName)
                        }
                    }
                    "Constants" -> {
                        val constsRaw = CardulatorEngine.nativeGetConstants()
                        val lines = constsRaw.split("\n").filter { it.contains("=") }
                        for (line in lines) {
                            val name = line.split("=")[0].trim()
                            if (name != "pi" && name != "e") {
                                CardulatorEngine.nativeDeleteConstant(name)
                            }
                        }
                    }
                    "Scripts" -> {
                        userScripts.clear()
                    }
                }
                saveUserDataToStorage()
                refreshTabContent(activeTab)
                Toast.makeText(this, "All $activeTab deleted", Toast.LENGTH_SHORT).show()
            }
        }

        // Toggle e-history variables button (#)
        btnToggleEVars.setOnClickListener {
            showEVars = !showEVars
            getSharedPreferences("droidulator_prefs", MODE_PRIVATE)
                .edit()
                .putBoolean("show_evars", showEVars)
                .apply()
            refreshTabContent("Variables")
        }
    }

    private fun getUnitDimensions(unit: String): Set<String> {
        val u = unit.trim().trim('"', '\'').lowercase()
        if (u.isEmpty()) return emptySet()

        val dims = mutableSetOf<String>()
        if (u in setOf("mm", "m", "meter", "meters", "cm", "km", "mile", "miles", "yard", "yards", "foot", "feet", "inch", "inches", "in", "ft", "yd", "nauticalmile", "angstrom", "au")) {
            dims.add("length")
        }
        if (u in setOf("mm^2", "mm2", "std_mm2", "awg", "std_awg", "m^2", "m2", "cm^2", "cm2", "km^2", "km2", "hectare", "acre", "acres", "sqft", "sqin")) {
            dims.add("area_wire")
        }
        if (u in setOf("m^3", "m3", "cm^3", "cm3", "liter", "liters", "litre", "litres", "l", "ml", "dl", "cl", "gal", "gallon", "gallons", "quart", "quarts", "pint", "pints", "cup", "cups", "floz", "tbsp", "tsp", "barrel")) {
            dims.add("volume")
        }
        if (u in setOf("mmhg", "torr", "atm", "bar", "mbar", "pa", "pascal", "kpa", "mpa", "gpa", "psi", "psf", "inhg")) {
            dims.add("pressure")
        }
        if (u in setOf("ohm", "ohms", "mohm", "kohm", "mohm", "gohm", "smd3", "smd4", "eia96") ||
            u.matches(Regex("""^\d{3,4}$""")) ||
            u.matches(Regex("""^\d{2}[a-z]$""", RegexOption.IGNORE_CASE)) ||
            u.matches(Regex("""^[0-9]+[rkm][0-9]*$""", RegexOption.IGNORE_CASE))) {
            dims.add("resistance")
        }
        if (u in setOf("farad", "f", "uf", "nf", "pf")) {
            dims.add("capacitance")
        }
        if (u in setOf("henry", "h", "uh", "mh", "nh")) {
            dims.add("inductance")
        }
        if (u in setOf("ampere", "amp", "amps", "a", "ma", "ua", "ka")) {
            dims.add("current")
        }
        if (u in setOf("volt", "volts", "v", "mv", "uv", "kv", "dbv", "dbmv", "dbuv", "dbu")) {
            dims.add("voltage")
        }
        if (u in setOf("watt", "watts", "w", "mw", "uw", "kw", "gw", "hp", "horsepower", "dbm", "dbw")) {
            dims.add("power")
        }
        if (u in setOf("db", "times", "ratio", "dbm", "dbw", "dbv", "dbmv", "dbuv", "dbu")) {
            dims.add("db_ratio")
        }
        if (u in setOf("hz", "khz", "mhz", "ghz")) {
            dims.add("frequency")
        }
        if (u in setOf("kg", "kilogram", "kilograms", "g", "gram", "grams", "mg", "ug", "ton", "tonne", "tonnes", "lb", "pound", "pounds", "oz", "ounce", "ounces", "stone", "carat")) {
            dims.add("mass")
        }
        if (u in setOf("s", "sec", "second", "seconds", "ms", "us", "ns", "min", "minute", "minutes", "hr", "hour", "hours", "day", "days", "week", "weeks", "month", "months", "year", "years")) {
            dims.add("time")
        }
        if (u in setOf("m/s", "km/hr", "km/h", "kph", "mph", "knot", "knots", "mach")) {
            dims.add("speed")
        }
        if (u in setOf("m/s^2", "m/s2", "cm/s^2", "cm/s2", "km/s^2", "km/s2", "km/h^2", "km/h2", "km/hr^2", "km/hr2", "ft/s^2", "ft/s2", "fps2", "in/s^2", "in/s2", "gal", "g0", "gee", "gravity", "g")) {
            dims.add("acceleration")
        }
        if (u in setOf("joule", "joules", "j", "kj", "mj", "cal", "calorie", "calories", "kcal", "ev", "kev", "mev", "gev", "wh", "kwh", "mwh", "btu")) {
            dims.add("energy")
        }
        if (u in setOf("newton", "newtons", "n", "kn", "dyn", "dyne", "lbf", "kgf")) {
            dims.add("force")
        }
        if (u in setOf("degc", "degf", "kelvin", "k")) {
            dims.add("temperature")
        }
        if (u in setOf("deg", "degree", "degrees", "rad", "radian", "radians")) {
            dims.add("angle")
        }
        if (u in setOf("coulomb", "tesla", "weber")) {
            dims.add("em")
        }
        return dims
    }

    private fun setupNavigationButtons() {
        // Left arrow: Move cursor left in input box
        btnNavLeft.setOnClickListener {
            val sel = etInput.selectionStart
            if (sel > 0) {
                etInput.setSelection(sel - 1)
            }
            etInput.requestFocus()
        }

        // Right arrow: Move cursor right in input box
        btnNavRight.setOnClickListener {
            val sel = etInput.selectionStart
            if (sel < etInput.text.length) {
                etInput.setSelection(sel + 1)
            }
            etInput.requestFocus()
        }

        // Up arrow: Scroll history UP in REPL (Recalls previous command including ones with errors)
        btnNavUp.setOnClickListener {
            if (tabs[currentTabPosition] == "REPL" && replHistory.isNotEmpty()) {
                if (replHistoryIndex == -1) {
                    tempReplInput = etInput.text.toString()
                    replHistoryIndex = replHistory.size - 1
                } else if (replHistoryIndex > 0) {
                    replHistoryIndex--
                }
                if (replHistoryIndex in replHistory.indices) {
                    etInput.setText(replHistory[replHistoryIndex])
                    etInput.setSelection(etInput.text.length)
                }
            }
            etInput.requestFocus()
        }

        // Down arrow: Scroll history DOWN in REPL
        btnNavDown.setOnClickListener {
            if (tabs[currentTabPosition] == "REPL" && replHistoryIndex != -1) {
                if (replHistoryIndex < replHistory.size - 1) {
                    replHistoryIndex++
                    etInput.setText(replHistory[replHistoryIndex])
                    etInput.setSelection(etInput.text.length)
                } else {
                    replHistoryIndex = -1
                    etInput.setText(tempReplInput)
                    etInput.setSelection(etInput.text.length)
                }
            }
            etInput.requestFocus()
        }
    }

    private fun setupErrorOverlayHandlers() {
        // Any click on the screen/overlay dismisses error state and restores input
        tvErrorOverlay.setOnClickListener {
            dismissErrorOverlay()
        }
        mainRoot.setOnClickListener {
            dismissErrorOverlay()
        }
        scrollView.setOnClickListener {
            dismissErrorOverlay()
        }
        tvOutput.setOnClickListener {
            dismissErrorOverlay()
        }
    }

    private fun setupSettingsHandlers() {
        rgThemeMode.setOnCheckedChangeListener { _, checkedId ->
            themeMode = when (checkedId) {
                R.id.rb_theme_dark -> 1
                R.id.rb_theme_light -> 2
                else -> 0
            }
            getSharedPreferences("droidulator_prefs", MODE_PRIVATE).edit().putInt("theme_mode", themeMode).apply()
            applyTheme()
        }

        cbDigitGrouping.setOnCheckedChangeListener { _, isChecked ->
            isDigitGroupingActive = isChecked
            updateFormatSettings()
        }

        cbHideWelcome.setOnCheckedChangeListener { _, isChecked ->
            hideWelcomeMessage = isChecked
            getSharedPreferences("droidulator_prefs", MODE_PRIVATE).edit().putBoolean("hide_welcome", isChecked).apply()
            rebuildReplOutput()
        }

        cbAutoPair.setOnCheckedChangeListener { _, isChecked ->
            isAutoPairEnabled = isChecked
            getSharedPreferences("droidulator_prefs", MODE_PRIVATE).edit().putBoolean("auto_pair", isChecked).apply()
        }

        btnSettingsEnterAction.setOnClickListener {
            toggleEnterAction()
        }

        btnSettingsClearHistory.setOnClickListener {
            clearSessionAndHistory()
        }

        btnLoadCustomTheme.setOnClickListener {
            openCustomThemeFilePicker()
        }

        btnResetCustomTheme.setOnClickListener {
            ThemePalette.resetToBuiltIn()
            val prefs = getSharedPreferences("droidulator_prefs", MODE_PRIVATE)
            prefs.edit().remove("custom_theme_json").apply()
            applyTheme()
            Toast.makeText(this, "Reset to built-in Gruvbox theme", Toast.LENGTH_SHORT).show()
        }
    }

    private fun updateFormatSettings() {
        CardulatorEngine.nativeSetFormatSettings(fmtMode, fmtPrecision, isDigitGroupingActive)

        val prefs = getSharedPreferences("droidulator_prefs", MODE_PRIVATE)
        prefs.edit()
            .putInt("fmt_mode", fmtMode)
            .putInt("fmt_precision", fmtPrecision)
            .putBoolean("digit_grouping", isDigitGroupingActive)
            .apply()

        rebuildReplOutput()
    }

    private fun formatHighlightedReplEntry(input: String, res: String, isError: Boolean): android.text.SpannableStringBuilder {
        val entrySsb = android.text.SpannableStringBuilder()

        // 1. Prompt prefix "> "
        val promptStart = entrySsb.length
        entrySsb.append("> ")
        entrySsb.setSpan(
            ForegroundColorSpan(ThemePalette.fgDim),
            promptStart,
            entrySsb.length,
            Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
        )

        // 2. Input expression with full syntax highlighting
        val exprStart = entrySsb.length
        entrySsb.append(input)
        applyExpressionSyntaxHighlighting(entrySsb, exprStart, input)

        entrySsb.append("\n")

        // 3. Output / Result line
        val resStart = entrySsb.length
        if (isError) {
            entrySsb.append("= ").append(res).append("\n")
            entrySsb.setSpan(
                ForegroundColorSpan(ThemePalette.red),
                resStart,
                entrySsb.length,
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
            )
        } else {
            if (input.startsWith("print(") || input.startsWith("plot(")) {
                entrySsb.append("= ").append(res).append("\n")
            } else {
                entrySsb.append(res).append("\n")
            }
            entrySsb.setSpan(
                ForegroundColorSpan(ThemePalette.green),
                resStart,
                entrySsb.length,
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
            )
        }

        return entrySsb
    }

    private fun rebuildReplOutput() {
        val ssb = android.text.SpannableStringBuilder()
        if (!hideWelcomeMessage) {
            val welcomeStart = ssb.length
            ssb.append(WELCOME_MSG)
            ssb.setSpan(
                ForegroundColorSpan(ThemePalette.fgDim),
                welcomeStart,
                ssb.length,
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
            )
        }

        if (replHistory.isNotEmpty()) {
            val historyCopy = ArrayList(replHistory)
            CardulatorEngine.nativeClearHistory()

            for (input in historyCopy) {
                val res = CardulatorEngine.nativeEvaluate(input)
                val isError = res.startsWith("ERROR: ")
                ssb.append(formatHighlightedReplEntry(input, res, isError))
            }
        }

        tvOutput.text = ssb
        scrollView.post {
            scrollView.fullScroll(ScrollView.FOCUS_DOWN)
        }
    }

    private fun appendOutputEntry(input: String, res: String, isError: Boolean) {
        val entrySsb = formatHighlightedReplEntry(input, res, isError)
        val currentText = tvOutput.text
        val ssb = android.text.SpannableStringBuilder(currentText)
        ssb.append(entrySsb)
        tvOutput.text = ssb
        scrollView.post {
            scrollView.fullScroll(ScrollView.FOCUS_DOWN)
        }
    }

    private fun appendRawOutput(text: String) {
        val currentText = tvOutput.text
        val ssb = android.text.SpannableStringBuilder(currentText)
        ssb.append(text)
        tvOutput.text = ssb
        scrollView.post {
            scrollView.fullScroll(ScrollView.FOCUS_DOWN)
        }
    }

    private fun toggleEnterAction() {
        isEnterSendMode = !isEnterSendMode
        updateEnterActionButtonText()
    }

    private fun updateEnterActionButtonText() {
        btnSettingsEnterAction.text = if (isEnterSendMode) {
            "Enter Key: Send Command"
        } else {
            "Enter Key: New Line"
        }
    }

    private fun toggleDegRad() {
        val isDeg = tvMode.text == "DEG"
        val nextDeg = !isDeg
        CardulatorEngine.nativeSetDegRad(nextDeg)
        tvMode.text = if (nextDeg) "DEG" else "RAD"
    }

    private fun clearSessionAndHistory() {
        CardulatorEngine.nativeClearHistory()
        replHistory.clear()
        replHistoryIndex = -1
        tvOutput.text = if (hideWelcomeMessage) "History & Session Cleared.\n" else "History & Session Cleared.\n$WELCOME_MSG"
        etInput.setText("")
        dismissErrorOverlay()
        Toast.makeText(this, "History & Session Cleared", Toast.LENGTH_SHORT).show()
    }

    private fun showErrorOverlay(errorMessage: String) {
        tvErrorOverlay.text = "ERROR: $errorMessage\n(Tap to dismiss)"
        tvErrorOverlay.visibility = View.VISIBLE
    }

    private fun dismissErrorOverlay() {
        if (tvErrorOverlay.visibility == View.VISIBLE) {
            tvErrorOverlay.visibility = View.GONE
        }
    }

    private fun submitCurrentInput() {
        val input = etInput.text.toString().trim()
        if (input.isEmpty()) return

        val activeTab = tabs[currentTabPosition]
        if (activeTab == "Scripts") {
            val log = CardulatorEngine.nativeRunScript(input)
            appendRawOutput("> SCRIPT OUTPUT:\n$log\n")
            etInput.setText("")
        } else {
            val res = CardulatorEngine.nativeEvaluate(input)
            val isError = res.startsWith("ERROR: ")
            if (isError) {
                val errDetails = res.substring(7)
                showErrorOverlay(errDetails)
            } else {
                dismissErrorOverlay()
            }
            appendOutputEntry(input, res, isError)

            // Always add to history (even if command had an error, so Up Arrow can recall it)
            if (replHistory.isEmpty() || replHistory.last() != input) {
                replHistory.add(input)
            }
            replHistoryIndex = -1
            etInput.setText("")

            if (!res.startsWith("ERROR: ")) {
                // Switch to Plot tab if a plot command was issued successfully
                if (input.startsWith("plot(") || input.startsWith("plot.")) {
                    val plotIdx = tabs.indexOf("Plot")
                    if (plotIdx != -1) {
                        spinnerTabs.setSelection(plotIdx)
                    }
                }
            }
        }
        etInput.requestFocus()
        etInput.post {
            etInput.requestFocus()
            val imm = getSystemService(INPUT_METHOD_SERVICE) as? android.view.inputmethod.InputMethodManager
            imm?.showSoftInput(etInput, android.view.inputmethod.InputMethodManager.SHOW_FORCED)
        }
    }

    private fun refreshTabContent(tabName: String) {
        dismissErrorOverlay()
        btnRunScript.visibility = View.GONE
        btnAddItem.visibility = View.GONE
        btnDeleteAll.visibility = View.GONE
        btnToggleEVars.visibility = View.GONE

        when (tabName) {
            "REPL", "Help" -> {
                scrollSettings.visibility = View.GONE
                scrollItems.visibility = View.GONE
                plotView.visibility = View.GONE
                scrollView.visibility = View.VISIBLE
                etInput.visibility = View.VISIBLE
                toolbarKeyboard.visibility = View.VISIBLE

                if (tabName == "Help") {
                    val help = CardulatorEngine.nativeGetHelp("")
                    tvOutput.text = "\n--- Help & Functions ---\n$help\n"
                }
            }
            "Plot" -> {
                scrollView.visibility = View.GONE
                scrollSettings.visibility = View.GONE
                scrollItems.visibility = View.GONE
                plotView.visibility = View.VISIBLE
                etInput.visibility = View.GONE
                toolbarKeyboard.visibility = View.GONE

                val rawData = CardulatorEngine.nativeGetPlotLines()
                plotView.updatePlotData(rawData)
            }
            "Settings" -> {
                scrollView.visibility = View.GONE
                scrollItems.visibility = View.GONE
                plotView.visibility = View.GONE
                scrollSettings.visibility = View.VISIBLE
                etInput.visibility = View.GONE
                toolbarKeyboard.visibility = View.GONE

                updateEnterActionButtonText()
                when (themeMode) {
                    1 -> rbThemeDark.isChecked = true
                    2 -> rbThemeLight.isChecked = true
                    else -> rbThemeAuto.isChecked = true
                }
            }
            "Variables", "Formulas", "Constants", "Scripts" -> {
                scrollView.visibility = View.GONE
                scrollSettings.visibility = View.GONE
                plotView.visibility = View.GONE
                scrollItems.visibility = View.VISIBLE
                etInput.visibility = View.GONE
                toolbarKeyboard.visibility = View.GONE
                btnAddItem.visibility = View.VISIBLE
                btnDeleteAll.visibility = View.VISIBLE
                if (tabName == "Variables") {
                    btnToggleEVars.visibility = View.VISIBLE
                }

                renderDedicatedMenuList(tabName)
            }
        }
    }

    private fun showConfirmDeleteDialog(itemName: String, onConfirmDelete: () -> Unit) {
        val fgColor = ThemePalette.fgText
        val tvMsg = TextView(this).apply {
            text = "Are you sure you want to delete '$itemName'?"
            setTextColor(fgColor)
            textSize = 14f
            typeface = android.graphics.Typeface.MONOSPACE
        }

        showGruvboxCustomDialog("Confirm Deletion", tvMsg, positiveText = "DELETE") {
            onConfirmDelete()
            Toast.makeText(this, "Deleted '$itemName'", Toast.LENGTH_SHORT).show()
        }
    }

    private fun renderDedicatedMenuList(tabName: String) {
        itemsContainer.removeAllViews()

        val cardBgColor = ThemePalette.bgCard
        val textColor = ThemePalette.fgText
        val dimColor = ThemePalette.fgDim

        when (tabName) {
            "Variables" -> {
                val varsRaw = CardulatorEngine.nativeGetVariables()
                var lines = varsRaw.split("\n").filter { it.contains("=") }
                if (!showEVars) {
                    lines = lines.filter { line ->
                        val varName = line.split("=")[0].trim()
                        !(varName.length > 1 && varName[0] == 'e' && varName.substring(1).all { it.isDigit() })
                    }
                }

                if (lines.isEmpty()) {
                    val emptyTv = TextView(this).apply {
                        text = "No user variables defined.\nClick '+ Add' above to create one."
                        setTextColor(dimColor)
                        textSize = 14f
                        setPadding(16, 32, 16, 32)
                    }
                    itemsContainer.addView(emptyTv)
                } else {
                    for (line in lines) {
                        val parts = line.split("=")
                        val name = parts[0].trim()
                        val value = if (parts.size > 1) parts[1].trim() else ""

                        val cardView = createMenuItemCard(
                            title = name,
                            subtitle = "= $value",
                            cardBgColor = cardBgColor,
                            textColor = textColor,
                            dimColor = dimColor,
                            onEdit = {
                                showDefinitionEditorDialog("Edit Variable '$name'", "$name = $value", "$name = $value") { newExpr ->
                                    CardulatorEngine.nativeEvaluate(newExpr)
                                    saveUserDataToStorage()
                                    refreshTabContent(tabName)
                                }
                            },
                            onDelete = {
                                showConfirmDeleteDialog(name) {
                                    CardulatorEngine.nativeDeleteVariable(name)
                                    saveUserDataToStorage()
                                    refreshTabContent(tabName)
                                }
                            }
                        )
                        itemsContainer.addView(cardView)
                    }
                }
            }
            "Constants" -> {
                val constsRaw = CardulatorEngine.nativeGetConstants()
                val lines = constsRaw.split("\n").filter { it.contains("=") }

                for (line in lines) {
                    val parts = line.split("=")
                    val name = parts[0].trim()
                    val value = if (parts.size > 1) parts[1].trim() else ""
                    val isBuiltIn = (name == "pi" || name == "e")

                    val cardView = createMenuItemCard(
                        title = name,
                        subtitle = "= $value ${if (isBuiltIn) "(Built-in Constant)" else ""}",
                        cardBgColor = cardBgColor,
                        textColor = textColor,
                        dimColor = dimColor,
                        onEdit = if (!isBuiltIn) {
                            {
                                showDefinitionEditorDialog("Edit Constant '$name'", "$name = $value", "$name = $value") { newExpr ->
                                    val fullExpr = if (newExpr.startsWith("const ")) newExpr else "const $newExpr"
                                    CardulatorEngine.nativeEvaluate(fullExpr)
                                    saveUserDataToStorage()
                                    refreshTabContent(tabName)
                                }
                            }
                        } else null,
                        onDelete = if (!isBuiltIn) {
                            {
                                showConfirmDeleteDialog(name) {
                                    CardulatorEngine.nativeDeleteConstant(name)
                                    saveUserDataToStorage()
                                    refreshTabContent(tabName)
                                }
                            }
                        } else null
                    )
                    itemsContainer.addView(cardView)
                }
            }
            "Formulas" -> {
                val formulasRaw = CardulatorEngine.nativeGetFormulas()
                val lines = formulasRaw.split("\n").filter { it.contains("=") || it.contains("(") }

                if (lines.isEmpty()) {
                    val emptyTv = TextView(this).apply {
                        text = "No custom formulas defined.\nClick '+ Add' above to create a formula e.g. f(x) = x^2"
                        setTextColor(dimColor)
                        textSize = 14f
                        setPadding(16, 32, 16, 32)
                    }
                    itemsContainer.addView(emptyTv)
                } else {
                    for (line in lines) {
                        val parts = line.split("=")
                        val name = parts[0].trim()
                        val bodySubtitle = if (parts.size > 1) "= " + parts[1].trim() else line

                        val cardView = createMenuItemCard(
                            title = name,
                            subtitle = bodySubtitle,
                            cardBgColor = cardBgColor,
                            textColor = textColor,
                            dimColor = dimColor,
                            onRun = {
                                showRunFormulaDialog(line)
                            },
                            onEdit = {
                                showDefinitionEditorDialog("Edit Formula '$name'", line, line) { newFormula ->
                                    CardulatorEngine.nativeEvaluate(newFormula)
                                    saveUserDataToStorage()
                                    refreshTabContent(tabName)
                                }
                            },
                            onDelete = {
                                showConfirmDeleteDialog(name) {
                                    val cleanName = name.split("(")[0].trim()
                                    CardulatorEngine.nativeDeleteFormula(cleanName)
                                    saveUserDataToStorage()
                                    refreshTabContent(tabName)
                                }
                            }
                        )
                        itemsContainer.addView(cardView)
                    }
                }
            }
            "Scripts" -> {
                if (userScripts.isEmpty()) {
                    val emptyTv = TextView(this).apply {
                        text = "No custom scripts available.\nClick '+ Add' above to create a new script."
                        setTextColor(dimColor)
                        textSize = 14f
                        setPadding(16, 32, 16, 32)
                    }
                    itemsContainer.addView(emptyTv)
                } else {
                    for (i in userScripts.indices) {
                        val script = userScripts[i]
                        val scriptName = script.first
                        val scriptCode = script.second

                        val cardView = createScriptMenuItemCard(
                            scriptName = scriptName,
                            scriptCode = scriptCode,
                            cardBgColor = cardBgColor,
                            textColor = textColor,
                            dimColor = dimColor,
                            onRun = {
                                val log = CardulatorEngine.nativeRunScript(scriptCode)
                                appendRawOutput("> SCRIPT OUTPUT ($scriptName):\n$log\n")
                                showScriptOutputDialog("Script Output: $scriptName", log)
                            },
                            onEdit = {
                                showFullScriptEditorDialog(scriptName, scriptCode) { newName, newCode ->
                                    userScripts[i] = Pair(newName, newCode)
                                    CardulatorEngine.nativeRegisterScript(newName, newCode)
                                    saveUserDataToStorage()
                                    refreshTabContent(tabName)
                                }
                            },
                            onDelete = {
                                showConfirmDeleteDialog(scriptName) {
                                    userScripts.removeAt(i)
                                    saveUserDataToStorage()
                                    refreshTabContent(tabName)
                                }
                            }
                        )
                        itemsContainer.addView(cardView)
                    }
                }
            }
        }
    }

    private fun createScriptMenuItemCard(
        scriptName: String,
        scriptCode: String,
        cardBgColor: Int,
        textColor: Int,
        dimColor: Int,
        onRun: () -> Unit,
        onEdit: () -> Unit,
        onDelete: () -> Unit
    ): View {
        val container = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(cardBgColor)
            setPadding(16, 12, 16, 12)
            val lp = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            lp.setMargins(0, 0, 0, 12)
            layoutParams = lp
        }

        val tvTitle = TextView(this).apply {
            text = scriptName
            setTextColor(textColor)
            textSize = 15f
            typeface = android.graphics.Typeface.MONOSPACE
            setTypeface(typeface, android.graphics.Typeface.BOLD)
        }

        val tvSubtitle = TextView(this).apply {
            val preview = scriptCode.lines().firstOrNull()?.take(60) ?: ""
            text = preview
            setTextColor(dimColor)
            textSize = 13f
            typeface = android.graphics.Typeface.MONOSPACE
        }

        val btnBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            val lp = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            lp.setMargins(0, 8, 0, 0)
            layoutParams = lp
        }

        val btnRun = Button(this).apply {
            text = "▶ Run"
            setTextColor(ThemePalette.bgDarkText)
            backgroundTintList = ColorStateList.valueOf(ThemePalette.yellow)
            typeface = android.graphics.Typeface.MONOSPACE
            textSize = 12f
            setOnClickListener { onRun() }
        }

        val btnEdit = Button(this).apply {
            text = "✎ Edit"
            setTextColor(ThemePalette.bgDarkText)
            backgroundTintList = ColorStateList.valueOf(ThemePalette.green)
            typeface = android.graphics.Typeface.MONOSPACE
            textSize = 12f
            val lp = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            lp.setMargins(16, 0, 0, 0)
            layoutParams = lp
            setOnClickListener { onEdit() }
        }

        val btnDelete = Button(this).apply {
            text = "✕ Delete"
            setTextColor(ThemePalette.bgDarkText)
            backgroundTintList = ColorStateList.valueOf(ThemePalette.red)
            typeface = android.graphics.Typeface.MONOSPACE
            textSize = 12f
            val lp = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            lp.setMargins(16, 0, 0, 0)
            layoutParams = lp
            setOnClickListener { onDelete() }
        }

        btnBar.addView(btnRun)
        btnBar.addView(btnEdit)
        btnBar.addView(btnDelete)

        container.addView(tvTitle)
        container.addView(tvSubtitle)
        container.addView(btnBar)

        return container
    }

    private fun createMenuItemCard(
        title: String,
        subtitle: String,
        cardBgColor: Int,
        textColor: Int,
        dimColor: Int,
        onRun: (() -> Unit)? = null,
        onEdit: (() -> Unit)?,
        onDelete: (() -> Unit)?
    ): View {
        val container = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(cardBgColor)
            setPadding(16, 12, 16, 12)
            val lp = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            lp.setMargins(0, 0, 0, 12)
            layoutParams = lp
        }

        val tvTitle = TextView(this).apply {
            text = title
            setTextColor(textColor)
            textSize = 15f
            typeface = android.graphics.Typeface.MONOSPACE
            setTypeface(typeface, android.graphics.Typeface.BOLD)
        }

        val tvSubtitle = TextView(this).apply {
            text = subtitle
            setTextColor(dimColor)
            textSize = 13f
            typeface = android.graphics.Typeface.MONOSPACE
        }

        container.addView(tvTitle)
        container.addView(tvSubtitle)

        if (onRun != null || onEdit != null || onDelete != null) {
            val btnBar = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                val lp = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                )
                lp.setMargins(0, 8, 0, 0)
                layoutParams = lp
            }

            if (onRun != null) {
                val btnRun = Button(this).apply {
                    text = "▶ Eval"
                    setTextColor(ThemePalette.bgDarkText)
                    backgroundTintList = ColorStateList.valueOf(ThemePalette.yellow)
                    typeface = android.graphics.Typeface.MONOSPACE
                    textSize = 12f
                    setOnClickListener { onRun() }
                }
                btnBar.addView(btnRun)
            }

            if (onEdit != null) {
                val btnEdit = Button(this).apply {
                    text = "✎ Edit"
                    setTextColor(ThemePalette.bgDarkText)
                    backgroundTintList = ColorStateList.valueOf(ThemePalette.green)
                    typeface = android.graphics.Typeface.MONOSPACE
                    textSize = 12f
                    val lp = LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT
                    )
                    if (onRun != null) lp.setMargins(16, 0, 0, 0)
                    layoutParams = lp
                    setOnClickListener { onEdit() }
                }
                btnBar.addView(btnEdit)
            }

            if (onDelete != null) {
                val btnDelete = Button(this).apply {
                    text = "✕ Delete"
                    setTextColor(ThemePalette.bgDarkText)
                    backgroundTintList = ColorStateList.valueOf(ThemePalette.red)
                    typeface = android.graphics.Typeface.MONOSPACE
                    textSize = 12f
                    val lp = LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT
                    )
                    lp.setMargins(16, 0, 0, 0)
                    layoutParams = lp
                    setOnClickListener { onDelete() }
                }
                btnBar.addView(btnDelete)
            }

            container.addView(btnBar)
        }

        return container
    }

    private fun showGruvboxCustomDialog(
        title: String,
        contentView: View,
        positiveText: String? = "SAVE",
        showCancel: Boolean = true,
        customButtons: List<View>? = null,
        onPositive: (() -> Unit)? = null
    ): Dialog {
        val dialog = Dialog(this, androidx.appcompat.R.style.Theme_AppCompat_DayNight_NoActionBar)
        val bgColor = ThemePalette.bg
        val bgSoft = ThemePalette.bgSoft
        val fgColor = ThemePalette.fgText
        val btnAqua = ThemePalette.aqua
        val borderColor = ThemePalette.borderColor

        dialog.window?.apply {
            setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            val dialogDrawable = GradientDrawable().apply {
                setColor(bgColor)
                setStroke(2, borderColor)
                cornerRadius = 16f
            }
            setBackgroundDrawable(dialogDrawable)
            setDimAmount(0.6f)
            addFlags(android.view.WindowManager.LayoutParams.FLAG_DIM_BEHIND)
            statusBarColor = bgSoft
            val controller = androidx.core.view.WindowCompat.getInsetsController(this, decorView)
            controller.isAppearanceLightStatusBars = isLightThemeActive
        }

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(bgColor)
            setPadding(36, 32, 36, 32)
        }

        val tvTitle = TextView(this).apply {
            text = title
            setTextColor(fgColor)
            textSize = 16f
            typeface = android.graphics.Typeface.MONOSPACE
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            setPadding(0, 0, 0, 16)
        }

        root.addView(tvTitle)

        val contentContainer = if (contentView.parent == null) {
            contentView
        } else {
            contentView
        }
        root.addView(contentContainer)

        val btnBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = android.view.Gravity.END
            setPadding(0, 20, 0, 0)
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
                val btnCancel = Button(this).apply {
                    text = "Cancel"
                    setTextColor(fgColor)
                    setBackgroundColor(Color.TRANSPARENT)
                    typeface = android.graphics.Typeface.MONOSPACE
                    textSize = 13f
                    setOnClickListener { dialog.dismiss() }
                }
                btnBar.addView(btnCancel)
            }

            if (positiveText != null) {
                val btnOk = Button(this).apply {
                    text = positiveText
                    setTextColor(ThemePalette.fgInverted)
                    backgroundTintList = ColorStateList.valueOf(btnAqua)
                    typeface = android.graphics.Typeface.MONOSPACE
                    textSize = 13f
                    setTypeface(typeface, android.graphics.Typeface.BOLD)
                    val lp = LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT)
                    if (showCancel) lp.setMargins(16, 0, 0, 0)
                    layoutParams = lp
                    setOnClickListener {
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
        return dialog
    }

    private fun showScriptOutputDialog(title: String, logText: String) {
        val bgSoft = ThemePalette.bgSoft
        val fgColor = ThemePalette.fgText
        val borderColor = ThemePalette.borderColor

        val tvOut = TextView(this).apply {
            text = if (logText.isBlank()) "(No output printed)" else logText
            setTextColor(fgColor)
            textSize = 13f
            typeface = android.graphics.Typeface.MONOSPACE
            setPadding(20, 16, 20, 16)
        }

        val scrollContainer = ScrollView(this).apply {
            val bgDrawable = GradientDrawable().apply {
                setColor(bgSoft)
                setStroke(2, borderColor)
                cornerRadius = 6f
            }
            background = bgDrawable
            isScrollbarFadingEnabled = false
            val lp = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 400)
            lp.setMargins(0, 8, 0, 8)
            layoutParams = lp
            addView(tvOut)
        }

        showGruvboxCustomDialog(title, scrollContainer, positiveText = "CLOSE", showCancel = false) {}
    }

    private fun showRunFormulaDialog(formulaLine: String) {
        val formulaHeader = formulaLine.split("=")[0].trim()
        val cleanName = if (formulaHeader.contains("(")) formulaHeader.substring(0, formulaHeader.indexOf("(")).trim() else formulaHeader

        var params = emptyList<String>()
        if (formulaHeader.contains("(") && formulaHeader.contains(")")) {
            val paramStr = formulaHeader.substring(formulaHeader.indexOf("(") + 1, formulaHeader.indexOf(")")).trim()
            if (paramStr.isNotEmpty()) {
                params = paramStr.split(",").map { it.trim() }.filter { it.isNotEmpty() }
            }
        }

        val bgSoft = ThemePalette.bgSoft
        val fgColor = ThemePalette.fgText
        val fgDim = ThemePalette.fgDim
        val borderColor = ThemePalette.borderColor

        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
        }

        val inputFields = mutableListOf<Pair<String, EditText>>()

        for (param in params) {
            val et = EditText(this).apply {
                hint = "Value for $param"
                typeface = android.graphics.Typeface.MONOSPACE
                setBackgroundColor(bgSoft)
                setTextColor(fgColor)
                setHintTextColor(fgDim)
                setPadding(16, 12, 16, 12)
                val lp = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
                lp.setMargins(0, 0, 0, 12)
                layoutParams = lp
            }
            layout.addView(et)
            inputFields.add(Pair(param, et))
        }

        val tvResultLabel = TextView(this).apply {
            text = "Result:"
            setTextColor(fgDim)
            textSize = 12f
            typeface = android.graphics.Typeface.MONOSPACE
            setPadding(0, 8, 0, 4)
        }

        val tvResultValue = TextView(this).apply {
            text = "="
            setTextColor(fgColor)
            textSize = 15f
            typeface = android.graphics.Typeface.MONOSPACE
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            setPadding(20, 14, 20, 14)
            val bgDrawable = GradientDrawable().apply {
                setColor(bgSoft)
                setStroke(2, borderColor)
                cornerRadius = 6f
            }
            background = bgDrawable
        }

        var currentEvalExpr = ""
        var currentRes = ""

        fun updateLiveResult() {
            val argValues = inputFields.map { it.second.text.toString().trim() }
            val hasEmptyInput = argValues.any { it.isEmpty() }
            if (hasEmptyInput && params.isNotEmpty()) {
                currentEvalExpr = if (argValues.isEmpty()) "$cleanName()" else "$cleanName(${argValues.joinToString(", ")})"
                currentRes = "Enter arguments above"
                tvResultValue.text = currentRes
                return
            }

            currentEvalExpr = if (argValues.isEmpty()) "$cleanName()" else "$cleanName(${argValues.joinToString(", ")})"
            // Preview: evaluate without modifying REPL history
            val rawRes = CardulatorEngine.nativeEvaluatePreview(currentEvalExpr)

            currentRes = if (rawRes.startsWith("ERROR")) rawRes else rawRes
            tvResultValue.text = "$currentEvalExpr = $currentRes"
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

        val btnExport = Button(this).apply {
            text = "EXPORT"
            setTextColor(ThemePalette.fgInverted)
            backgroundTintList = ColorStateList.valueOf(ThemePalette.aqua)
            typeface = android.graphics.Typeface.MONOSPACE
            textSize = 13f
            setTypeface(typeface, android.graphics.Typeface.BOLD)
            val lp = LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            lp.setMargins(16, 0, 0, 0)
            layoutParams = lp
        }

        var formulaDialog: Dialog? = null

        val btnClose = Button(this).apply {
            text = "CLOSE"
            setTextColor(fgColor)
            setBackgroundColor(Color.TRANSPARENT)
            typeface = android.graphics.Typeface.MONOSPACE
            textSize = 13f
            setOnClickListener { formulaDialog?.dismiss() }
        }

        btnExport.setOnClickListener {
            if (currentEvalExpr.isNotEmpty() && !currentRes.startsWith("Enter")) {
                val res = CardulatorEngine.nativeEvaluate(currentEvalExpr)
                val isErr = res.startsWith("ERROR: ")
                appendOutputEntry(currentEvalExpr, res, isErr)
                if (replHistory.isEmpty() || replHistory.last() != currentEvalExpr) {
                    replHistory.add(currentEvalExpr)
                }
                saveUserDataToStorage()
                Toast.makeText(this@MainActivity, "Exported to REPL", Toast.LENGTH_SHORT).show()
                refreshTabContent(tabs[currentTabPosition])
            }
        }

        formulaDialog = showGruvboxCustomDialog(
            formulaLine,
            layout,
            customButtons = listOf(btnClose, btnExport)
        )
    }

    private fun showDefinitionEditorDialog(title: String, initialExpr: String, hintText: String, onSave: (String) -> Unit) {
        val bgSoft = ThemePalette.bgSoft
        val fgColor = ThemePalette.fgText
        val fgDim = ThemePalette.fgDim

        val et = EditText(this).apply {
            setText(initialExpr)
            setSelection(initialExpr.length)
            hint = hintText
            typeface = android.graphics.Typeface.MONOSPACE
            setBackgroundColor(bgSoft)
            setTextColor(fgColor)
            setHintTextColor(fgDim)
            setPadding(20, 16, 20, 16)
            addTextChangedListener(object : TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
                    if (!isHighlighting && s != null) {
                        applySyntaxHighlighting(editableText)
                    }
                }
                override fun afterTextChanged(s: Editable?) {}
            })
        }

        if (initialExpr.isNotEmpty()) {
            et.post { applySyntaxHighlighting(et.editableText) }
        }

        showGruvboxCustomDialog(title, et, positiveText = "SAVE") {
            val valStr = et.text.toString().trim()
            if (valStr.isNotEmpty()) {
                onSave(valStr)
            }
        }
    }

    private fun showFullScriptEditorDialog(initialName: String, initialCode: String, onSave: (String, String) -> Unit) {
        val dialog = Dialog(this, R.style.GruvboxFullscreenDialog)
        dialog.setContentView(R.layout.dialog_script_editor)

        val root = dialog.findViewById<LinearLayout>(R.id.script_editor_root)
        val btnCancel = dialog.findViewById<Button>(R.id.btn_script_cancel)
        val btnTab = dialog.findViewById<Button>(R.id.btn_script_tab)
        val btnRun = dialog.findViewById<Button>(R.id.btn_script_run)
        val btnSave = dialog.findViewById<Button>(R.id.btn_script_save)
        val etName = dialog.findViewById<EditText>(R.id.et_script_name)
        val etCode = dialog.findViewById<EditText>(R.id.et_script_code)

        val bgColor = ThemePalette.bg
        val bgSoft = ThemePalette.bgSoft
        val fgColor = ThemePalette.fgText
        val fgDim = ThemePalette.fgDim
        val btnAqua = ThemePalette.aqua

        root.setBackgroundColor(bgColor)

        etName.apply {
            setText(initialName)
            setBackgroundColor(bgSoft)
            setTextColor(fgColor)
            setHintTextColor(fgDim)
        }

        etCode.apply {
            setText(initialCode)
            setBackgroundColor(bgSoft)
            setTextColor(fgColor)
            setHintTextColor(fgDim)
            addTextChangedListener(object : TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {
                    if (!isHighlighting && s != null) {
                        applySyntaxHighlighting(editableText)
                    }
                }
                override fun afterTextChanged(s: Editable?) {}
            })
        }

        if (initialCode.isNotEmpty()) {
            etCode.post { applySyntaxHighlighting(etCode.editableText) }
        }

        btnCancel.apply {
            setTextColor(fgColor)
            setOnClickListener { dialog.dismiss() }
        }

        btnTab.apply {
            setTextColor(fgColor)
            setOnClickListener {
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
                    } else {
                        val outCursor = intArrayOf(prefix.length)
                        val completed = CardulatorEngine.nativeAutocomplete(prefix, prefix.length, outCursor)
                        if (completed != prefix) {
                            etCode.text.replace(wordStart, cursor, completed)
                            etCode.setSelection((wordStart + completed.length).coerceAtMost(etCode.text.length))
                        }
                    }
                }
            }
        }

        btnRun.apply {
            setTextColor(ThemePalette.bgDarkText)
            backgroundTintList = ColorStateList.valueOf(ThemePalette.yellow)
            setOnClickListener {
                val newName = etName.text.toString().trim()
                val newCode = etCode.text.toString().trim()
                if (newName.isNotEmpty() && newCode.isNotEmpty()) {
                    onSave(newName, newCode)
                    val log = CardulatorEngine.nativeRunScript(newCode)
                    appendRawOutput("> SCRIPT OUTPUT ($newName):\n$log\n")
                    showScriptOutputDialog("Script Output: $newName", log)
                } else {
                    Toast.makeText(this@MainActivity, "Name and code cannot be empty", Toast.LENGTH_SHORT).show()
                }
            }
        }

        btnSave.apply {
            setTextColor(ThemePalette.fgInverted)
            backgroundTintList = ColorStateList.valueOf(btnAqua)
            setOnClickListener {
                val newName = etName.text.toString().trim()
                val newCode = etCode.text.toString().trim()
                if (newName.isNotEmpty() && newCode.isNotEmpty()) {
                    onSave(newName, newCode)
                    dialog.dismiss()
                } else {
                    Toast.makeText(this@MainActivity, "Name and code cannot be empty", Toast.LENGTH_SHORT).show()
                }
            }
        }

        dialog.show()

        dialog.window?.apply {
            setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
            setBackgroundDrawable(ColorDrawable(bgColor))
            statusBarColor = bgSoft
            val controller = androidx.core.view.WindowCompat.getInsetsController(this, decorView)
            controller.isAppearanceLightStatusBars = isLightThemeActive
        }
    }

    private fun showAddItemDialog() {
        val activeTab = tabs[currentTabPosition]
        if (activeTab == "Scripts") {
            showFullScriptEditorDialog("", "") { name, code ->
                userScripts.add(Pair(name, code))
                CardulatorEngine.nativeRegisterScript(name, code)
                saveUserDataToStorage()
                refreshTabContent("Scripts")
            }
            return
        }

        val title = "Add $activeTab Item"
        val hintText = when (activeTab) {
            "Formulas" -> "f(x) = x^2 + 2*x"
            "Constants" -> "MY_CONST = 3.14159"
            else -> "x = 42"
        }

        showDefinitionEditorDialog(title, "", hintText) { expr ->
            when (activeTab) {
                "Constants" -> {
                    val fullExpr = if (expr.startsWith("const ")) expr else "const $expr"
                    CardulatorEngine.nativeEvaluate(fullExpr)
                }
                else -> {
                    CardulatorEngine.nativeEvaluate(expr)
                }
            }
            saveUserDataToStorage()
            refreshTabContent(activeTab)
        }
    }

    private fun saveUserDataToStorage() {
        try {
            val file = java.io.File(filesDir, "user_data.json")
            val json = org.json.JSONObject()

            // Save variables
            val varsRaw = CardulatorEngine.nativeGetVariables()
            val varLines = varsRaw.split("\n").filter { it.contains("=") }
            val varsArray = org.json.JSONArray()
            for (l in varLines) {
                val varName = l.split("=")[0].trim()
                if (!(varName.length > 1 && varName[0] == 'e' && varName.substring(1).all { it.isDigit() })) {
                    varsArray.put(l)
                }
            }
            json.put("variables", varsArray)

            // Save formulas
            val formulasRaw = CardulatorEngine.nativeGetFormulas()
            val formulaLines = formulasRaw.split("\n").filter { it.contains("=") || it.contains("(") }
            val formulasArray = org.json.JSONArray()
            for (l in formulaLines) {
                formulasArray.put(l)
            }
            json.put("formulas", formulasArray)

            // Save constants
            val constsRaw = CardulatorEngine.nativeGetConstants()
            val constLines = constsRaw.split("\n").filter { it.contains("=") }
            val constsArray = org.json.JSONArray()
            for (l in constLines) {
                val name = l.split("=")[0].trim()
                if (name != "pi" && name != "e") {
                    constsArray.put(l)
                }
            }
            json.put("constants", constsArray)

            // Save scripts
            val scriptsArray = org.json.JSONArray()
            for (s in userScripts) {
                val scriptObj = org.json.JSONObject()
                scriptObj.put("name", s.first)
                scriptObj.put("code", s.second)
                scriptsArray.put(scriptObj)
            }
            json.put("scripts", scriptsArray)

            file.writeText(json.toString())
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun loadUserDataFromStorage() {
        try {
            val file = java.io.File(filesDir, "user_data.json")
            if (!file.exists()) return

            val json = org.json.JSONObject(file.readText())

            // Restore variables
            if (json.has("variables")) {
                val varsArray = json.getJSONArray("variables")
                for (i in 0 until varsArray.length()) {
                    val line = varsArray.getString(i)
                    CardulatorEngine.nativeEvaluate(line)
                }
            }

            // Restore formulas
            if (json.has("formulas")) {
                val formulasArray = json.getJSONArray("formulas")
                for (i in 0 until formulasArray.length()) {
                    val line = formulasArray.getString(i)
                    CardulatorEngine.nativeEvaluate(line)
                }
            }

            // Restore constants
            if (json.has("constants")) {
                val constsArray = json.getJSONArray("constants")
                for (i in 0 until constsArray.length()) {
                    val line = constsArray.getString(i)
                    val parts = line.split("=")
                    if (parts.size >= 2) {
                        val name = parts[0].trim()
                        val valStr = parts[1].trim()
                        CardulatorEngine.nativeEvaluate("const $name = $valStr")
                    }
                }
            }

            // Restore scripts
            if (json.has("scripts")) {
                val scriptsArray = json.getJSONArray("scripts")
                userScripts.clear()
                for (i in 0 until scriptsArray.length()) {
                    val obj = scriptsArray.getJSONObject(i)
                    val name = obj.getString("name")
                    val code = obj.getString("code")
                    userScripts.add(Pair(name, code))
                    // Register each script so run(name) can find it
                    CardulatorEngine.nativeRegisterScript(name, code)
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }



    private val customThemeLauncher = registerForActivityResult(androidx.activity.result.contract.ActivityResultContracts.GetContent()) { uri ->
        if (uri != null) {
            try {
                contentResolver.openInputStream(uri)?.use { inputStream ->
                    val jsonStr = inputStream.bufferedReader(Charsets.UTF_8).readText()
                    if (ThemePalette.loadCustomThemeJson(jsonStr)) {
                        val prefs = getSharedPreferences("droidulator_prefs", MODE_PRIVATE)
                        prefs.edit().putString("custom_theme_json", jsonStr).apply()
                        applyTheme()
                        Toast.makeText(this, "Custom JSON theme loaded successfully!", Toast.LENGTH_SHORT).show()
                    } else {
                        Toast.makeText(this, "Invalid theme JSON format", Toast.LENGTH_LONG).show()
                    }
                }
            } catch (e: Exception) {
                Toast.makeText(this, "Failed to load file: ${e.message}", Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun openCustomThemeFilePicker() {
        customThemeLauncher.launch("*/*")
    }

    // --- Theme Management (Dark / Light / System Auto) ---
    private fun applyTheme() {
        val currentNightMode = resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK
        val isSystemDark = currentNightMode == Configuration.UI_MODE_NIGHT_YES

        isLightThemeActive = when (themeMode) {
            1 -> false // Dark
            2 -> true  // Light
            else -> !isSystemDark // Auto: Follow system
        }

        ThemePalette.isDark = !isLightThemeActive

        val bgColor = ThemePalette.bg
        val bgSoftColor = ThemePalette.bgSoft
        val fgColor = ThemePalette.fgText
        val fgDimColor = ThemePalette.fgDim
        val spinnerTint = ThemePalette.green

        // Root & Top Bar
        mainRoot.setBackgroundColor(bgColor)
        topHeaderBar.setBackgroundColor(bgSoftColor)
        spinnerTabs.backgroundTintList = ColorStateList.valueOf(spinnerTint)
        setupTabSpinner()

        // Scroll Views & Plot View
        scrollView.setBackgroundColor(bgColor)
        tvOutput.setTextColor(fgColor)
        scrollSettings.setBackgroundColor(bgColor)
        scrollItems.setBackgroundColor(bgColor)
        plotView.setThemeMode(isLightThemeActive)

        // Settings View Elements
        tvSettingsSectionTheme.setTextColor(fgColor)
        tvSettingsSectionCustomTheme.setTextColor(fgColor)
        tvSettingsSectionDisplay.setTextColor(fgColor)
        tvSettingsSectionEnterAction.setTextColor(fgColor)

        rbThemeAuto.setTextColor(fgColor)
        rbThemeDark.setTextColor(fgColor)
        rbThemeLight.setTextColor(fgColor)

        cbHideWelcome.setTextColor(fgColor)
        cbAutoPair.setTextColor(fgColor)
        cbDigitGrouping.setTextColor(fgColor)

        btnResetCustomTheme.setTextColor(fgColor)
        btnResetCustomTheme.backgroundTintList = ColorStateList.valueOf(bgSoftColor)

        // Input Field & Keyboard Bar
        etInput.setBackgroundColor(bgSoftColor)
        etInput.setTextColor(fgColor)
        etInput.setHintTextColor(fgDimColor)
        toolbarKeyboard.setBackgroundColor(bgSoftColor)

        // Re-render menu cards under new theme if active tab is a menu list
        val currentTab = tabs[currentTabPosition]
        if (currentTab in arrayOf("Variables", "Formulas", "Constants", "Scripts")) {
            renderDedicatedMenuList(currentTab)
        }

        // Re-apply syntax highlighting for input field if non-empty
        if (etInput.text.isNotEmpty()) {
            applySyntaxHighlighting(etInput.text)
        }
    }

    // --- Syntax Highlighting & Bracket Matching ---
    private fun applyExpressionSyntaxHighlighting(ssb: android.text.SpannableStringBuilder, startOffset: Int, text: String) {
        val numColor = ThemePalette.purple
        val funcColor = ThemePalette.aqua
        val constColor = ThemePalette.yellow
        val keywordColor = ThemePalette.red
        val opColor = ThemePalette.orange
        val errColor = ThemePalette.red

        // 1. Highlight numbers
        for (match in NUMBER_REGEX.findAll(text)) {
            ssb.setSpan(
                ForegroundColorSpan(numColor),
                startOffset + match.range.first,
                startOffset + match.range.last + 1,
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
            )
        }

        // 2. Highlight control flow keywords
        for (match in KEYWORD_REGEX.findAll(text)) {
            ssb.setSpan(
                ForegroundColorSpan(keywordColor),
                startOffset + match.range.first,
                startOffset + match.range.last + 1,
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
            )
        }

        // 3. Highlight functions
        for (match in FUNC_REGEX.findAll(text)) {
            ssb.setSpan(
                ForegroundColorSpan(funcColor),
                startOffset + match.range.first,
                startOffset + match.range.last + 1,
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
            )
        }

        // 4. Highlight built-in constants & history variables
        for (match in CONST_REGEX.findAll(text)) {
            ssb.setSpan(
                ForegroundColorSpan(constColor),
                startOffset + match.range.first,
                startOffset + match.range.last + 1,
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
            )
        }

        // 5. Highlight operators
        for (match in OP_REGEX.findAll(text)) {
            ssb.setSpan(
                ForegroundColorSpan(opColor),
                startOffset + match.range.first,
                startOffset + match.range.last + 1,
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
            )
        }

        // 6. Rainbow Bracket Highlighting & Matching
        val colors = ThemePalette.rainbowColors
        val numColors = colors.size
        var stackSize = 0
        val maxStack = 64
        val charStack = CharArray(maxStack)
        val posStack = IntArray(maxStack)

        for (i in text.indices) {
            val c = text[i]
            val absPos = startOffset + i
            if (c == '(' || c == '[' || c == '{') {
                val color = colors[stackSize % numColors]
                ssb.setSpan(ForegroundColorSpan(color), absPos, absPos + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                if (stackSize < maxStack) {
                    charStack[stackSize] = c
                    posStack[stackSize] = absPos
                    stackSize++
                }
            } else if (c == ')' || c == ']' || c == '}') {
                if (stackSize > 0) {
                    val topChar = charStack[stackSize - 1]
                    stackSize--
                    val matches = (topChar == '(' && c == ')') ||
                                  (topChar == '[' && c == ']') ||
                                  (topChar == '{' && c == '}')
                    if (matches) {
                        val color = colors[stackSize % numColors]
                        ssb.setSpan(ForegroundColorSpan(color), absPos, absPos + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                    } else {
                        ssb.setSpan(ForegroundColorSpan(errColor), absPos, absPos + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                    }
                } else {
                    ssb.setSpan(ForegroundColorSpan(errColor), absPos, absPos + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
                }
            }
        }

        while (stackSize > 0) {
            stackSize--
            val unclosedPos = posStack[stackSize]
            ssb.setSpan(ForegroundColorSpan(errColor), unclosedPos, unclosedPos + 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
        }
    }

    private fun applySyntaxHighlighting(editable: Editable) {
        isHighlighting = true

        val existingSpans = editable.getSpans(0, editable.length, ForegroundColorSpan::class.java)
        for (span in existingSpans) {
            editable.removeSpan(span)
        }

        val text = editable.toString()
        val dummySsb = android.text.SpannableStringBuilder(text)
        applyExpressionSyntaxHighlighting(dummySsb, 0, text)

        val newSpans = dummySsb.getSpans(0, dummySsb.length, ForegroundColorSpan::class.java)
        for (span in newSpans) {
            val start = dummySsb.getSpanStart(span)
            val end = dummySsb.getSpanEnd(span)
            val flags = dummySsb.getSpanFlags(span)
            editable.setSpan(ForegroundColorSpan(span.foregroundColor), start, end, flags)
        }

        isHighlighting = false
    }
}
