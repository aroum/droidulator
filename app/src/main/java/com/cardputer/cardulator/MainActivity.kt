package com.cardputer.cardulator

import androidx.activity.OnBackPressedCallback
import androidx.core.widget.doAfterTextChanged
import androidx.core.view.doOnPreDraw
import android.app.Dialog
import android.content.res.ColorStateList
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.ViewModelProvider


data class ReplDisplayItem(
        val input: String,
        val rawOutput: String,
        val isError: Boolean,
        val hasNum: Boolean = false,
        val numVal: Double = 0.0,
        val prefix: String = "",
        val suffix: String = "",
        val arrayValues: DoubleArray? = null,
        val isRawText: Boolean = false
    )


class MainActivity : AppCompatActivity() {

    private val dialogs by lazy { CalculatorDialogs(this, ::trackDialog) }
    private val activeDialogs = mutableSetOf<Dialog>()

    private fun trackDialog(dialog: Dialog): Dialog {
        activeDialogs.add(dialog)
        dialog.setOnDismissListener { activeDialogs.remove(dialog) }
        return dialog
    }

    private val pendingInputEdits = mutableSetOf<Runnable>()

    private fun postInputEdit(action: () -> Unit) {
        lateinit var task: Runnable
        task = Runnable {
            pendingInputEdits.remove(task)
            if (!isDestroyed && !isFinishing && etInput.isAttachedToWindow) action()
        }
        pendingInputEdits.add(task)
        etInput.post(task)
    }

    private var startupDelivery: CompletionDelivery<Throwable?>? = null
    private var pendingPlot: java.util.concurrent.Future<*>? = null

    override fun onDestroy() {
        startupDelivery?.cancel()
        if (::etInput.isInitialized) pendingInputEdits.forEach { etInput.removeCallbacks(it) }
        pendingInputEdits.clear()
        pendingPlot?.cancel(false)
        ++plotRefreshGeneration
        if (replOutputDelegate.isInitialized()) replOutput.close()
        if (helpPanelDelegate.isInitialized()) helpPanel.close()
        activeDialogs.toList().forEach { it.dismiss() }
        activeDialogs.clear()
        super.onDestroy()
    }

    private lateinit var mainRoot: LinearLayout
    private lateinit var topHeaderBar: LinearLayout
    private lateinit var spinnerTabs: Spinner
    private lateinit var tvMode: TextView
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
    private lateinit var btnEnter: Button

    // Settings UI elements
    private lateinit var rgThemeMode: RadioGroup
    private lateinit var rbThemeAuto: RadioButton
    private lateinit var rbThemeDark: RadioButton
    private lateinit var rbThemeLight: RadioButton
    private lateinit var btnLoadCustomTheme: Button
    private lateinit var btnResetCustomTheme: Button
    private lateinit var btnSettingsEnterAction: CheckBox
    private lateinit var btnSettingsClearHistory: Button
    private lateinit var cbDigitGrouping: CheckBox
    private lateinit var cbHideWelcome: CheckBox
    private lateinit var cbAutoPair: CheckBox
    private lateinit var tvSettingsSectionTheme: TextView
    private lateinit var tvSettingsSectionCustomTheme: TextView
    private lateinit var tvSettingsSectionDisplay: TextView

    // Number Formatting Spinners
    private lateinit var spinnerFmtMode: Spinner
    private lateinit var spinnerFmtPrec: Spinner
    private val fmtModeItems = arrayOf("NORM", "FLT", "FIX", "SCI", "ENG")
    private val fmtModeItemsDisplay = arrayOf("NORM", "FLT", "FIX", "SCI", "ENG")
    private val fmtPrecItems = arrayOf("0dp", "1dp", "2dp", "3dp", "4dp", "5dp", "6dp", "7dp", "8dp", "9dp", "10dp", "11dp", "12dp")
    private val fmtPrecItemsDisplay = arrayOf("0dp", "1dp", "2dp", "3dp", "4dp", "5dp", "6dp", "7dp", "8dp", "9dp", "10dp", "11dp", "12dp")

    private var fmtMode = 0
    private var fmtPrecision = 6
    private var isDigitGroupingActive = false
    private var hideWelcomeMessage = false
    private var isAutoPairEnabled = true

    // Enter Key Action: true = Send command, false = New line
    private var isEnterSendMode = true

    private var currentTabPosition = 0
    private var plotRefreshGeneration = 0L
    private val tabs = arrayOf("REPL", "Scripts", "Formulas", "Constants", "Variables", "Plot", "Settings", "Help", "Units")
    private val tabsDisplay get() = arrayOf(R.string.tab_repl_display, R.string.tab_scripts_display, R.string.tab_formulas_display, R.string.tab_constants_display, R.string.tab_variables_display, R.string.tab_plot_display, R.string.tab_settings_display, R.string.help_tab, R.string.units_tab).map(::getString).toTypedArray()
    private fun tabTitle(tab: String): String = when (tab) {
        "REPL" -> getString(R.string.tab_repl)
        "Scripts" -> getString(R.string.tab_scripts)
        "Formulas" -> getString(R.string.tab_formulas)
        "Constants" -> getString(R.string.tab_constants)
        "Variables" -> getString(R.string.tab_variables)
        "Plot" -> getString(R.string.tab_plot_display).removeSuffix("")
        "Settings" -> getString(R.string.tab_settings)
        else -> tab
    }

    private val session by lazy { ViewModelProvider(this)[CalculatorSession::class.java] }
    private val userScripts get() = session.userScripts
    private val scriptLibrary get() = session.scriptLibrary

    // Theme Mode: 0 = Auto (System), 1 = Dark, 2 = Light
    private val themeSettings by lazy { CalculatorThemeSettings(applicationContext) }
    private var themeMode = 0
    private var isLightThemeActive = false
    private lateinit var btnDeleteAll: Button
    private lateinit var btnToggleEVars: Button

    private var showEVars = false

    // REPL History management
    private val replHistory get() = session.replHistory
    private val replDisplayItems get() = session.replDisplayItems
    private var replHistoryIndex = -1
    private var tempReplInput = ""

    // Flag to avoid infinite loop in TextWatcher during syntax highlighting
    private var isHighlighting = false
    private val highlighter = CalculatorSyntaxHighlighter()
    private var isSubmitting = false
    private var autocompleteEditor: EditText? = null

    private fun selectAutocompleteEditor(editor: EditText) {
        if (autocompleteEditor !== editor) {
            session.resetAutocomplete()
            autocomplete.reset()
            autocompleteEditor = editor
        }
    }

    private val autocomplete by lazy { session.newAutocompleteController() }

    private val welcomeMessage get() = getString(R.string.ui_text_026)

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density + 0.5f).toInt()

    private fun applySystemInsets(view: View) {
        if (android.os.Build.VERSION.SDK_INT < 35) return
        val left = view.paddingLeft
        val top = view.paddingTop
        val right = view.paddingRight
        val bottom = view.paddingBottom
        androidx.core.view.ViewCompat.setOnApplyWindowInsetsListener(view) { target, insets ->
            val bars = insets.getInsets(androidx.core.view.WindowInsetsCompat.Type.systemBars() or androidx.core.view.WindowInsetsCompat.Type.displayCutout())
            val ime = insets.getInsets(androidx.core.view.WindowInsetsCompat.Type.ime())
            val bottomInset = maxOf(bars.bottom, ime.bottom)
            target.setPadding(left + bars.left, top + bars.top, right + bars.right, bottom + bottomInset)
            insets
        }
        androidx.core.view.ViewCompat.requestApplyInsets(view)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        // Bind the complete screen before its first visible frame.
        findViewById<View>(R.id.main_root).visibility = View.INVISIBLE
        startupDelivery = UnitsDatabase.initialize(applicationContext) { failure ->
            if (isFinishing || isDestroyed) return@initialize
            when (failure) {
                null -> finishStartup()
                is UnsatisfiedLinkError -> {
                    android.util.Log.e("Droidulator", "Unable to load calculator engine", failure)
                    AlertDialog.Builder(this).setMessage(R.string.engine_load_error)
                        .setPositiveButton(android.R.string.ok) { _, _ -> finish() }
                        .setOnCancelListener { finish() }.show()
                }
                else -> {
                    android.util.Log.e("Droidulator", "Unable to install units data", failure)
                    Toast.makeText(this, R.string.ui_text_001, Toast.LENGTH_LONG).show()
                    finish()
                }
            }
        }
    }

    private fun finishStartup() {
        if (session.commandRunning) {
            session.commandResult.observe(this, object : androidx.lifecycle.Observer<CalculatorSession.CommandResult?> {
                override fun onChanged(value: CalculatorSession.CommandResult?) {
                    if (value == null) return
                    session.commandResult.removeObserver(this)
                    finishStartup()
                }
            })
            return
        }
        if (!session.initialized) {
            if (!session.initialize(applicationContext))
                Toast.makeText(this, R.string.ui_text_019, Toast.LENGTH_LONG).show()
        }

        // Bind Views
        mainRoot = findViewById(R.id.main_root)
        applySystemInsets(mainRoot)
        topHeaderBar = mainRoot.getChildAt(0) as LinearLayout
        spinnerTabs = findViewById(R.id.spinner_tabs)
        tvMode = findViewById(R.id.tv_mode)
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
        btnEnter = findViewById(R.id.btn_enter)

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
        themeMode = themeSettings.mode
        isEnterSendMode = prefs.getBoolean("enter_send_mode", true)
        val savedDegMode = prefs.getBoolean("degrees_mode", true)

        cbDigitGrouping.isChecked = isDigitGroupingActive
        cbHideWelcome.isChecked = hideWelcomeMessage
        cbAutoPair.isChecked = isAutoPairEnabled
        session.setAngleMode(savedDegMode)
        tvMode.text = if (savedDegMode) "DEG" else "RAD"
        tvMode.contentDescription = getString(R.string.angle_mode_accessibility, tvMode.text)
        updateEnterActionButtonText()
        updateInputImeOptions()
        if (hideWelcomeMessage) {
            tvOutput.text = ""
        } else {
            tvOutput.text = welcomeMessage
        }

        listOf(rbThemeAuto, rbThemeDark, rbThemeLight).getOrNull(themeMode)?.isChecked = true
        if (themeMode !in 0..2) rbThemeAuto.isChecked = true

        themeSettings.restoreCustomPalette()

        setupTabSpinner()
        setupFormatSpinners()
        setupInputHandlers()
        setupControlButtons()
        setupNavigationButtons()
        setupErrorOverlayHandlers()
        setupSettingsHandlers()

        // Apply theme upon creation and restore the retained REPL output.
        applyTheme()
        rebuildReplOutput()
        mainRoot.visibility = View.VISIBLE
        mainRoot.doOnPreDraw { reportFullyDrawn() }
        observeCommands()
        session.definitionDraft?.let { definitionController.show(it.title, it.expression, it.hint, it.tab) }
        session.formulaRunDraft?.let { formulaRunner.show(it.formula, it.arguments) }
        session.scriptDraft?.let { openScriptEditor(it.originalName) }
    }

    private fun rounded(color: Int) = GradientDrawable().apply {
        setColor(color)
        cornerRadius = dp(8).toFloat()
    }

    private fun spinnerColor(spinner: Spinner): Int = when (spinner) {
        spinnerTabs -> ThemePalette.green
        spinnerFmtMode -> ThemePalette.orange
        else -> ThemePalette.yellow
    }

    private fun closeSearch() {
        searchField.text.clear()
        searchField.visibility = View.GONE
        spinnerTabs.visibility = View.VISIBLE
        hideSoftKeyboard()
    }

    private fun spinnerAdapter(items: Array<String>, spinner: Spinner) =
        object : ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, items) {
            private fun row(position: Int, popup: Boolean) = TextView(this@MainActivity).apply {
                text = items[position]
                typeface = android.graphics.Typeface.MONOSPACE
                textSize = 13f
                gravity = android.view.Gravity.CENTER
                minHeight = dp(36)
                setSingleLine()
                setPadding(dp(4), dp(8), dp(4), dp(8))
                val selected = popup && position == spinner.selectedItemPosition
                setTextColor(if (selected || !popup) ThemePalette.btnText else spinnerColor(spinner))
                if (selected) background = rounded(spinnerColor(spinner))
            }
            override fun getView(position: Int, convertView: View?, parent: ViewGroup): View = row(position, false)
            override fun getDropDownView(position: Int, convertView: View?, parent: ViewGroup): View = row(position, true)
        }

    private val menuCards by lazy { CalculatorMenuCards(this) }

    private val searchableTabs = setOf("Variables", "Formulas", "Constants", "Scripts", "Help", "Units")
    private val searchField get() = findViewById<EditText>(R.id.search_query)
    private val definitionController by lazy {
        DefinitionEditorController(this, session, dialogs, highlighter, { expression, draft ->
            session.runCommand(expression, definition = draft)
        }, { activeDialogs.remove(it) })
    }

    private val formulaRunner by lazy {
        FormulaRunner(this, session, dialogs, { queueCommand(it) }, { activeDialogs.remove(it) })
    }

    private val replOutputDelegate = lazy {
        ReplOutputController(
            session, ReplOutputRenderer(highlighter), tvOutput, scrollView,
            welcome = { welcomeMessage to !hideWelcomeMessage },
            formatSettings = { Triple(fmtMode, fmtPrecision, isDigitGroupingActive) }
        )
    }
    private val replOutput by replOutputDelegate

    private val libraryPanel by lazy { LibraryPanel(this, itemsContainer, menuCards) }

    private val helpPanelDelegate = lazy { HelpPanel(itemsContainer, menuCards, dialogs, session) }
    private val helpPanel by helpPanelDelegate

    private val unitsPanel by lazy { UnitsPanel(this, itemsContainer, menuCards, dialogs) }

    private fun renderUnits() = unitsPanel.render(searchField.text.toString())

    private fun filterCards() {
        if (tabs[currentTabPosition] == "Units") {
            renderUnits()
            return
        }
        val words = searchField.text.toString().trim().split(Regex("\\s+")).filter { it.isNotEmpty() }
        for (index in 0 until itemsContainer.childCount) {
            val card = itemsContainer.getChildAt(index)
            val content = card.tag as? String ?: ""
            card.visibility = if (words.all { content.contains(it, ignoreCase = true) }) View.VISIBLE else View.GONE
        }
    }

    private fun setupTabSpinner() {
        for (spinner in listOf(spinnerTabs, spinnerFmtMode, spinnerFmtPrec)) {
            spinner.addOnLayoutChangeListener { _, left, _, right, _, _, _, _, _ ->
                if (right > left) {
                    spinner.dropDownWidth = right - left
                    spinner.dropDownHorizontalOffset = 0
                }
            }
        }
        spinnerTabs.adapter = spinnerAdapter(tabsDisplay, spinnerTabs)
        spinnerTabs.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                currentTabPosition = position
                refreshTabContent(tabs[position])
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if (searchField.visibility == View.VISIBLE) closeSearch()
                else {
                    isEnabled = false
                    onBackPressedDispatcher.onBackPressed()
                    isEnabled = true
                }
            }
        })
        val search = searchField
        findViewById<View>(R.id.search_toggle).setToolbarClickListener {
            val opening = search.visibility != View.VISIBLE
            spinnerTabs.visibility = if (opening) View.GONE else View.VISIBLE
            search.visibility = if (opening) View.VISIBLE else View.GONE
            if (opening) {
                search.requestFocus()
                (getSystemService(INPUT_METHOD_SERVICE) as android.view.inputmethod.InputMethodManager)
                    .showSoftInput(search, android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT)
            } else {
                closeSearch()
            }
        }
        search.doAfterTextChanged { filterCards() }
        search.setOnEditorActionListener { _, action, event ->
            val done = action == EditorInfo.IME_ACTION_DONE || event?.keyCode == KeyEvent.KEYCODE_ENTER
            if (done) hideSoftKeyboard()
            done
        }
    }

    private fun setupFormatSpinners() {
        spinnerFmtMode.adapter = spinnerAdapter(fmtModeItemsDisplay, spinnerFmtMode)
        spinnerFmtPrec.adapter = spinnerAdapter(fmtPrecItemsDisplay, spinnerFmtPrec)
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
                if (!isSubmitting) {
                    dismissErrorOverlay()
                }
                if (isHighlighting || s == null || (s is Editable && highlighter.isApplying(s))) return

                val currentText = s.toString()
                val composing = android.view.inputmethod.BaseInputConnection.getComposingSpanStart(etInput.editableText) >= 0

                // Intercept single Enter key press from soft keyboard in REPL when enter-to-send is active.
                // Pasted multiline text has count > 2 (or text without just a newline) and is preserved intact.
                val isSingleNewline = !composing && (
                    (count == 1 && start < currentText.length && (currentText[start] == '\n' || currentText[start] == '\r')) ||
                    (count == 2 && start + 1 < currentText.length && currentText.substring(start, start + 2) == "\r\n")
                )
                if (tabs[currentTabPosition] == "REPL" && isEnterSendMode && isSingleNewline) {
                    postInputEdit {
                        if (isSubmitting) return@postInputEdit
                        val text = etInput.editableText
                        val lenToDelete = if (count == 2) 2 else 1
                        if (start + lenToDelete <= text.length && text.subSequence(start, start + lenToDelete).any { it == '\n' || it == '\r' }) {
                            isHighlighting = true
                            try {
                                text.delete(start, start + lenToDelete)
                            } finally {
                                isHighlighting = false
                            }
                        }
                        submitCurrentInput()
                    }
                    return
                }

                // A pasted multiline expression must remain intact. Enter actions
                // are handled by the editor/key listeners above or the single newline interceptor.
                if (isAutoPairEnabled && !composing && count == 1 && before == 0 && start < currentText.length) {
                    val typed = currentText[start]
                    val next = currentText.getOrNull(start + 1)
                    val closing = when (typed) {
                        '(' -> ')'
                        '[' -> ']'
                        '{' -> '}'
                        '\"' -> '\"'
                        '\'' -> '\''
                        else -> null
                    }
                    if (next == typed && (typed == ')' || typed == ']' || typed == '}' || typed == '\"' || typed == '\'')) {
                        postInputEdit {
                            val text = etInput.editableText
                            if (etInput.selectionStart == start + 1 && start + 1 < text.length && text[start] == typed && text[start + 1] == typed) {
                                isHighlighting = true
                                try { text.delete(start, start + 1); etInput.setSelection(start + 1) }
                                finally { isHighlighting = false }
                                highlighter.apply(text)
                            }
                        }
                    } else if (closing != null) {
                        postInputEdit {
                            val text = etInput.editableText
                            if (etInput.selectionStart == start + 1 && start < text.length && text[start] == typed && android.view.inputmethod.BaseInputConnection.getComposingSpanStart(text) < 0) {
                                isHighlighting = true
                                try { text.insert(start + 1, closing.toString()); etInput.setSelection(start + 1) }
                                finally { isHighlighting = false }
                                highlighter.apply(text)
                            }
                        }
                    }
                }
            }

            override fun afterTextChanged(s: Editable?) {
                if (isHighlighting || s == null || (s is Editable && highlighter.isApplying(s))) return
                highlighter.apply(etInput.editableText)
            }
        })
    }

    private fun setupControlButtons() {
        btnTab.setToolbarClickListener {
            selectAutocompleteEditor(etInput)
            autocomplete.complete(etInput.text.toString(), etInput.selectionStart.coerceAtLeast(0))?.let { edit ->
                etInput.setText(edit.text)
                etInput.setSelection(edit.caret.coerceIn(0, edit.text.length))
            }
        }

        btnClear.setToolbarClickListener {
            etInput.setText("")
            dismissErrorOverlay()
        }
        btnEnter.setToolbarClickListener {
            if (tabs[currentTabPosition] == "REPL") {
                submitCurrentInput()
            }
        }
        btnClear.setOnLongClickListener {
            val message = TextView(this).apply {
                text = this@MainActivity.getString(R.string.ui_text_002)
                setTextColor(ThemePalette.fgText)
            }
            dialogs.show(getString(R.string.clear_session_title), message, positiveText = getString(R.string.action_clear)) {
                clearSessionAndHistory()
            }
            true
        }

        // Mode toggle (DEG / RAD)
        tvMode.setToolbarClickListener {
            toggleDegRad()
        }

        // Add Item button (Var / Const / Script)
        btnAddItem.setToolbarClickListener {
            showAddItemDialog()
        }

        // Delete All Items button (Red ✕)
        btnDeleteAll.setToolbarClickListener {
            val activeTab = tabs[currentTabPosition]
            val fgColor = ThemePalette.fgText
            val tvMsg = TextView(this).apply {
                text = if (activeTab == "Variables") getString(R.string.delete_all_variables)
                    else getString(R.string.delete_all_confirm, tabTitle(activeTab))
                setTextColor(fgColor)
                textSize = 14f
                typeface = android.graphics.Typeface.MONOSPACE
            }
            dialogs.show(getString(R.string.delete_all_title, tabTitle(activeTab)), tvMsg, positiveText = getString(R.string.action_delete_all)) {
                session.clearLibrary(activeTab)
                session.saveUserData()
                refreshTabContent(activeTab)
                Toast.makeText(this, getString(R.string.deleted_all, tabTitle(activeTab)), Toast.LENGTH_SHORT).show()
            }
        }

        // Toggle e-history variables button (#)
        btnToggleEVars.setToolbarClickListener {
            showEVars = !showEVars
            getSharedPreferences("droidulator_prefs", MODE_PRIVATE)
                .edit()
                .putBoolean("show_evars", showEVars)
                .apply()
            refreshTabContent("Variables")
        }
    }

    private fun setupNavigationButtons() {
        // Left arrow: Move cursor left in input box
        btnNavLeft.setToolbarClickListener {
            val sel = etInput.selectionStart
            if (sel > 0) {
                etInput.setSelection(sel - 1)
            }
            etInput.requestFocus()
        }

        // Right arrow: Move cursor right in input box
        btnNavRight.setToolbarClickListener {
            val sel = etInput.selectionStart
            if (sel < etInput.text.length) {
                etInput.setSelection(sel + 1)
            }
            etInput.requestFocus()
        }

        // Up arrow: Scroll history UP in REPL (Recalls previous command including ones with errors)
        btnNavUp.setToolbarClickListener {
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
        btnNavDown.setToolbarClickListener {
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
        etInput.setOnClickListener {
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
            themeSettings.mode = themeMode
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

        findViewById<CheckBox>(R.id.cb_toolbar_haptics).apply {
            isChecked = getSharedPreferences("droidulator_prefs", MODE_PRIVATE)
                .getBoolean("toolbar_haptics", false)
            setOnCheckedChangeListener { _, checked ->
                getSharedPreferences("droidulator_prefs", MODE_PRIVATE).edit()
                    .putBoolean("toolbar_haptics", checked).apply()
            }
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
            themeSettings.resetCustomPalette()
            applyTheme()
            Toast.makeText(this, this@MainActivity.getString(R.string.ui_text_003), Toast.LENGTH_SHORT).show()
        }
    }

    private fun updateFormatSettings() {
        val prefs = getSharedPreferences("droidulator_prefs", MODE_PRIVATE)
        prefs.edit()
            .putInt("fmt_mode", fmtMode)
            .putInt("fmt_precision", fmtPrecision)
            .putBoolean("digit_grouping", isDigitGroupingActive)
            .apply()

        rebuildReplOutput()
    }

    private fun rebuildReplOutput() = replOutput.rebuild()

    private fun appendOutputEntry(entry: ReplDisplayItem) = replOutput.append(entry)

    private fun appendRawOutput(text: String) = replOutput.append(
        ReplDisplayItem(input = "", rawOutput = text, isError = false, isRawText = true)
    )

    private fun toggleEnterAction() {
        isEnterSendMode = !isEnterSendMode
        getSharedPreferences("droidulator_prefs", MODE_PRIVATE)
            .edit()
            .putBoolean("enter_send_mode", isEnterSendMode)
            .apply()
        updateEnterActionButtonText()
        updateInputImeOptions()
    }

    private fun updateEnterActionButtonText() {
        btnSettingsEnterAction.isChecked = isEnterSendMode
        if (::btnEnter.isInitialized) {
            btnEnter.visibility = if (isEnterSendMode) View.GONE else View.VISIBLE
        }
    }

    private fun updateInputImeOptions() {
        if (!::etInput.isInitialized) return
        etInput.imeOptions = if (isEnterSendMode) {
            EditorInfo.IME_ACTION_SEND or EditorInfo.IME_FLAG_NO_FULLSCREEN
        } else {
            EditorInfo.IME_ACTION_NONE or EditorInfo.IME_FLAG_NO_ENTER_ACTION or EditorInfo.IME_FLAG_NO_FULLSCREEN
        }
        val imm = getSystemService(INPUT_METHOD_SERVICE) as? android.view.inputmethod.InputMethodManager
        imm?.restartInput(etInput)
    }

    private fun hideSoftKeyboard() {
        val imm = getSystemService(INPUT_METHOD_SERVICE) as? android.view.inputmethod.InputMethodManager
        val focusedView = currentFocus ?: etInput
        focusedView.clearFocus()
        imm?.hideSoftInputFromWindow(mainRoot.windowToken, 0)
        mainRoot.requestFocus()
    }

    private fun toggleDegRad() {
        val isDeg = session.angleMode()
        val nextDeg = !isDeg
        session.setAngleMode(nextDeg)
        tvMode.text = if (nextDeg) "DEG" else "RAD"
        tvMode.contentDescription = getString(R.string.angle_mode_accessibility, tvMode.text)
        getSharedPreferences("droidulator_prefs", MODE_PRIVATE)
            .edit()
            .putBoolean("degrees_mode", nextDeg)
            .apply()
    }

    private fun clearSessionAndHistory() {
        replOutput.invalidate()
        session.clearHistory()
        replHistory.clear()
        replDisplayItems.clear()
        replHistoryIndex = -1
        tempReplInput = ""
        autocomplete.reset()
        tvOutput.text = if (hideWelcomeMessage) getString(R.string.history_cleared) else getString(R.string.history_cleared) + welcomeMessage
        etInput.setText("")
        dismissErrorOverlay()
        session.saveUserData()
        Toast.makeText(this, this@MainActivity.getString(R.string.ui_text_004), Toast.LENGTH_SHORT).show()
        val activeTab = tabs[currentTabPosition]
        if (activeTab == "Variables" || activeTab == "Formulas" || activeTab == "Constants") {
            refreshTabContent(activeTab)
        }
    }

    private fun showErrorOverlay(errorMessage: String) {
        tvErrorOverlay.text = getString(R.string.error_overlay, errorMessage)
        tvErrorOverlay.visibility = View.VISIBLE
    }

    private fun dismissErrorOverlay() {
        if (tvErrorOverlay.visibility == View.VISIBLE) {
            tvErrorOverlay.visibility = View.GONE
        }
    }

    private fun submitCurrentInput() {
        val input = etInput.text.toString().trim()
        if (input.isEmpty() || !queueCommand(input)) return
        isSubmitting = true
        try { etInput.setText("") } finally { isSubmitting = false }
        etInput.requestFocus()
    }

    private fun queueCommand(input: String, scriptName: String? = null): Boolean {
        val accepted = session.runCommand(input, scriptName)
        if (accepted && (scriptName != null || tabs[currentTabPosition] != "REPL")) hideSoftKeyboard()
        return accepted
    }

    // Keep engine controls from waiting on the worker's native lock while a
    // mutating command is running. Rendering and system navigation stay free.
    override fun dispatchTouchEvent(event: android.view.MotionEvent): Boolean =
        if (readyForCommands && session.commandRunning) true else super.dispatchTouchEvent(event)


    private var readyForCommands = false

    private fun observeCommands() {
        readyForCommands = true
        session.commandResult.observe(this) { result ->
            if (result == null) return@observe
            session.commandResult.value = null
            val entry = result.entry
            val definition = result.definition
            if (definition != null) {
                definitionController.complete(definition, entry)
                if (!entry.isError && tabs[currentTabPosition] == definition.tab) refreshTabContent(definition.tab)
                return@observe
            }
            val script = result.scriptName
            if (script != null && !entry.isError) {
                val log = entry.rawOutput
                appendRawOutput(getString(R.string.script_output_entry, script, log))
                dialogs.showScriptOutput(getString(R.string.script_output_title, script), log)
            } else {
                appendOutputEntry(entry)
                if (entry.isError) showErrorOverlay(entry.rawOutput.removePrefix("ERROR: "))
                else dismissErrorOverlay()
            }
            replHistoryIndex = -1
            if (result.showPlot) spinnerTabs.setSelection(tabs.indexOf("Plot"))
        }
    }

    private fun refreshTabContent(tabName: String) {
        pendingPlot?.cancel(false)
        pendingPlot = null
        if (helpPanelDelegate.isInitialized()) helpPanel.cancel()
        findViewById<View>(R.id.search_toggle).visibility = if (tabName in searchableTabs) View.VISIBLE else View.GONE
        dismissErrorOverlay()
        btnAddItem.visibility = View.GONE
        btnDeleteAll.visibility = View.GONE
        btnToggleEVars.visibility = View.GONE
        if (tabName != "REPL") {
            hideSoftKeyboard()
        }

        when (tabName) {
            "REPL" -> {
                scrollSettings.visibility = View.GONE
                scrollItems.visibility = View.GONE
                plotView.visibility = View.GONE
                scrollView.visibility = View.VISIBLE
                etInput.visibility = View.VISIBLE
                toolbarKeyboard.visibility = View.VISIBLE
                updateInputImeOptions()
                updateEnterActionButtonText()
            }
            "Plot" -> {
                scrollView.visibility = View.GONE
                scrollSettings.visibility = View.GONE
                scrollItems.visibility = View.GONE
                plotView.visibility = View.VISIBLE
                etInput.visibility = View.GONE
                toolbarKeyboard.visibility = View.GONE

                val generation = ++plotRefreshGeneration
                pendingPlot = session.preparePlot { result ->
                    if (isDestroyed || isFinishing || tabs[currentTabPosition] != "Plot" ||
                        generation != plotRefreshGeneration
                    ) return@preparePlot
                    result.onSuccess { plotView.updatePlotData(it) }
                }
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
            "Variables", "Formulas", "Constants", "Scripts", "Help", "Units" -> {
                scrollView.visibility = View.GONE
                scrollSettings.visibility = View.GONE
                plotView.visibility = View.GONE
                scrollItems.visibility = View.VISIBLE
                etInput.visibility = View.GONE
                toolbarKeyboard.visibility = View.GONE
                btnAddItem.visibility = if (tabName == "Help" || tabName == "Units") View.GONE else View.VISIBLE
                btnDeleteAll.visibility = btnAddItem.visibility
                if (tabName == "Variables") {
                    btnToggleEVars.visibility = View.VISIBLE
                }

                renderDedicatedMenuList(tabName)
            }
        }
    }

    private fun renderDedicatedMenuList(tabName: String) {
        if (tabName == "Units") {
            renderUnits()
            return
        }
        itemsContainer.removeAllViews()

        when (tabName) {
            "Help" -> helpPanel.render(::filterCards)
            "Variables", "Constants" -> {
                val constants = tabName == "Constants"
                libraryPanel.renderValues(
                    values = session.libraryValues(constants),
                    constants = constants,
                    showHistory = showEVars,
                    onEdit = { name, expression ->
                        val title = if (constants) R.string.edit_constant else R.string.edit_variable
                        definitionController.show(getString(title, name), expression, expression, tabName)
                    },
                    onDelete = { name ->
                        dialogs.confirmDelete(name) {
                            session.deleteValue(name, constants)
                            session.saveUserData()
                            refreshTabContent(tabName)
                        }
                    }
                )
            }
            "Formulas" -> libraryPanel.renderFormulas(
                formulas = session.libraryFormulas(),
                onRun = { formulaRunner.show(it) },
                onEdit = { formula ->
                    definitionController.show(getString(R.string.edit_formula, formula.signature), formula.expression, formula.expression, tabName)
                },
                onDelete = { formula ->
                    dialogs.confirmDelete(formula.signature) {
                        session.deleteFormula(formula.name)
                        session.saveUserData()
                        refreshTabContent(tabName)
                    }
                }
            )
            "Scripts" -> libraryPanel.renderScripts(
                scripts = userScripts,
                onRun = { code, name -> queueCommand(code, name) },
                onEdit = { openScriptEditor(it) },
                onDelete = { name ->
                    dialogs.confirmDelete(name) {
                        scriptLibrary.remove(name)
                        session.saveUserData()
                        refreshTabContent(tabName)
                    }
                }
            )
        }
        filterCards()
    }

    private fun openScriptEditor(originalName: String?) {
        val draft = session.scriptDraft ?: ScriptDraft(
            originalName, originalName.orEmpty(),
            userScripts.firstOrNull { it.first == originalName }?.second.orEmpty()
        ).also { session.scriptDraft = it }
        val editor = ScriptEditor(
            this, session, highlighter, ::applySystemInsets, ::selectAutocompleteEditor,
            { code, name -> queueCommand(code, name) },
            { dialog -> activeDialogs.add(dialog) },
            { dialog ->
                activeDialogs.remove(dialog)
                if (!isChangingConfigurations && session.scriptDraft === draft) session.scriptDraft = null
            }
        )
        editor.show(draft) { name, code ->
            if (!scriptLibrary.save(draft.originalName, name, code)) {
                Toast.makeText(this, R.string.ui_text_008, Toast.LENGTH_LONG).show()
                return@show false
            }
            session.saveUserData()
            if (tabs[currentTabPosition] == "Scripts") refreshTabContent("Scripts")
            true
        }
    }

    private fun showAddItemDialog() {
        val activeTab = tabs[currentTabPosition]
        if (activeTab == "Scripts") {
            openScriptEditor(null)
            return
        }

        val title = getString(R.string.add_item_title, tabTitle(activeTab))
        val hintText = when (activeTab) {
            "Formulas" -> "f(x) = x^2 + 2*x"
            "Constants" -> "MY_CONST = 3.14159"
            else -> "x = 42"
        }

        definitionController.show(title, "", hintText, activeTab)
    }

    private val customThemeLauncher = registerForActivityResult(androidx.activity.result.contract.ActivityResultContracts.GetContent()) { uri ->
        if (uri != null) {
            try {
                if (themeSettings.importCustomPalette(uri)) {
                    applyTheme()
                    Toast.makeText(this, R.string.ui_text_020, Toast.LENGTH_SHORT).show()
                } else {
                    Toast.makeText(this, R.string.ui_text_021, Toast.LENGTH_LONG).show()
                }
            } catch (e: Exception) {
                Toast.makeText(this, getString(R.string.theme_load_failed, e.message.orEmpty()), Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun openCustomThemeFilePicker() {
        customThemeLauncher.launch("*/*")
    }

    // --- Theme Management (Dark / Light / System Auto) ---
    private fun applyTheme() {
        isLightThemeActive = themeSettings.isLight(resources.configuration, themeMode)

        ThemePalette.isDark = !isLightThemeActive

        val bgColor = ThemePalette.bg
        val bgSoftColor = ThemePalette.bgSoft
        val fgColor = ThemePalette.fgText
        val fgDimColor = ThemePalette.fgDim


        // Root & Top Bar
        mainRoot.setBackgroundColor(bgColor)
        topHeaderBar.background = rounded(bgSoftColor)
        window?.let { activityWindow ->
            if (android.os.Build.VERSION.SDK_INT < 35) {
                activityWindow.statusBarColor = bgColor
                activityWindow.navigationBarColor = bgColor
            }
            val controller = androidx.core.view.WindowCompat.getInsetsController(activityWindow, activityWindow.decorView)
            val lightBars = androidx.core.graphics.ColorUtils.calculateLuminance(bgColor) > 0.5
            controller.isAppearanceLightStatusBars = lightBars
            controller.isAppearanceLightNavigationBars = lightBars
        }
        val themedButtons = listOf(
            btnAddItem to ThemePalette.yellow, btnDeleteAll to ThemePalette.red, btnToggleEVars to ThemePalette.aqua,
            btnNavLeft to ThemePalette.blue, btnNavRight to ThemePalette.blue,
            btnNavUp to ThemePalette.purple, btnNavDown to ThemePalette.purple,
            btnTab to ThemePalette.yellow, btnClear to ThemePalette.red,
            btnEnter to ThemePalette.green,
            btnLoadCustomTheme to ThemePalette.aqua,
            btnSettingsClearHistory to ThemePalette.red
        )
        val btnFg = ThemePalette.btnText
        for ((button, color) in themedButtons) {
            button.backgroundTintList = ColorStateList.valueOf(color)
            button.setTextColor(btnFg)
            (button as? com.google.android.material.button.MaterialButton)?.iconTint = ColorStateList.valueOf(btnFg)
        }
        for (spinner in listOf(spinnerTabs, spinnerFmtMode, spinnerFmtPrec)) {
            spinner.background = rounded(spinnerColor(spinner))
            spinner.setPopupBackgroundDrawable(rounded(bgSoftColor))
        }
        (spinnerTabs.adapter as? ArrayAdapter<*>)?.notifyDataSetChanged()
        (spinnerFmtMode.adapter as? ArrayAdapter<*>)?.notifyDataSetChanged()
        (spinnerFmtPrec.adapter as? ArrayAdapter<*>)?.notifyDataSetChanged()

        // Scroll Views & Plot View
        scrollView.setBackgroundColor(bgColor)
        tvOutput.setTextColor(fgColor)
        tvMode.background = rounded(ThemePalette.yellow)
        tvMode.setTextColor(ThemePalette.btnText)
        tvMode.typeface = android.graphics.Typeface.MONOSPACE
        tvMode.textSize = 13f
        searchField.apply {
            background = rounded(ThemePalette.bgSoft)
            setTextColor(ThemePalette.fgText)
            setHintTextColor(ThemePalette.fgDim)
            typeface = android.graphics.Typeface.MONOSPACE
        }
        findViewById<com.google.android.material.button.MaterialButton>(R.id.search_toggle).apply {
            backgroundTintList = ColorStateList.valueOf(ThemePalette.aqua)
            iconTint = ColorStateList.valueOf(ThemePalette.btnText)
        }
        scrollSettings.setBackgroundColor(bgColor)
        scrollItems.setBackgroundColor(bgColor)
        plotView.setThemeMode(isLightThemeActive)

        // Settings View Elements
        tvSettingsSectionTheme.setTextColor(fgColor)
        tvSettingsSectionCustomTheme.setTextColor(fgColor)
        tvSettingsSectionDisplay.setTextColor(fgColor)

        val settingsTint = ColorStateList.valueOf(ThemePalette.aqua)
        listOf(rbThemeAuto, rbThemeDark, rbThemeLight, cbHideWelcome, cbAutoPair,
            cbDigitGrouping, btnSettingsEnterAction,
            findViewById<CheckBox>(R.id.cb_toolbar_haptics)).forEach { control ->
            control.setTextColor(fgColor)
            control.buttonTintList = settingsTint
        }

        btnResetCustomTheme.setTextColor(fgColor)
        btnResetCustomTheme.backgroundTintList = ColorStateList.valueOf(bgSoftColor)

        // Input Field & Keyboard Bar
        etInput.background = rounded(bgSoftColor)
        etInput.setTextColor(fgColor)
        etInput.setHintTextColor(fgDimColor)
        toolbarKeyboard.background = rounded(bgSoftColor)

        // Re-render menu cards under new theme if active tab is a menu list
        val currentTab = tabs[currentTabPosition]
        if (currentTab in arrayOf("Variables", "Formulas", "Constants", "Scripts", "Help", "Units")) {
            renderDedicatedMenuList(currentTab)
        }
        rebuildReplOutput()

        // Re-apply syntax highlighting for input field if non-empty
        if (etInput.text.isNotEmpty()) {
            highlighter.apply(etInput.text)
        }
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (readyForCommands && session.commandRunning && event.keyCode != KeyEvent.KEYCODE_BACK)
            return true
        if (event.action == KeyEvent.ACTION_DOWN) {
            dismissErrorOverlay()
        }
        return super.dispatchKeyEvent(event)
    }
}
