package com.cardputer.cardulator

import android.content.Context
import android.os.Build
import android.os.SystemClock
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.widget.CheckBox
import android.widget.EditText
import android.widget.Spinner
import android.widget.TextView
import androidx.lifecycle.ViewModelProvider
import androidx.test.core.app.ActivityScenario
import androidx.test.core.app.ApplicationProvider
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.Espresso.pressBack
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.closeSoftKeyboard
import androidx.test.espresso.action.ViewActions.replaceText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference
import org.junit.After
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/** These fixtures deliberately reset data and must only run on disposable emulators. */
@RunWith(AndroidJUnit4::class)
class AndroidRegressionTest {
    private lateinit var scenario: ActivityScenario<MainActivity>
    private val context get() = ApplicationProvider.getApplicationContext<Context>()

    @Before fun launchCleanEmulatorApp() {
        check(InstrumentationRegistry.getArguments().getString("ephemeralDevice") == "true")
        check(Build.HARDWARE in listOf("ranchu", "goldfish") || Build.FINGERPRINT.contains("generic"))
        for (name in listOf("user_data.json", "plot_state.bin"))
            for (suffix in listOf("", ".bak", ".new")) File(context.filesDir, name + suffix).delete()
        context.getSharedPreferences("droidulator_prefs", Context.MODE_PRIVATE).edit().clear()
            .putBoolean("hide_welcome", true).putBoolean("enter_send_mode", true).commit()
        launch()
    }

    @After fun closeActivity() {
        if (::scenario.isInitialized) scenario.close()
    }

    private fun launch() {
        scenario = ActivityScenario.launch(MainActivity::class.java)
        await("startup") { activity { session(it).initialized && it.findViewById<View>(R.id.main_root).visibility == View.VISIBLE } }
    }

    private fun session(activity: MainActivity) = ViewModelProvider(activity)[CalculatorSession::class.java]

    private fun <T> activity(block: (MainActivity) -> T): T {
        val value = AtomicReference<T>()
        scenario.onActivity { value.set(block(it)) }
        return value.get()
    }

    private fun <T> engine(operation: CardulatorEngine.() -> T): T {
        val completed = CountDownLatch(1)
        val value = AtomicReference<Result<T>>()
        activity { session(it).compute(operation) { result -> value.set(result); completed.countDown() } }
        assertTrue("engine operation timed out", completed.await(30, TimeUnit.SECONDS))
        return value.get().getOrThrow()
    }

    private fun await(description: String, condition: () -> Boolean) {
        val deadline = SystemClock.uptimeMillis() + 30_000
        while (SystemClock.uptimeMillis() < deadline) {
            if (condition()) return
            SystemClock.sleep(50)
        }
        fail("Timed out waiting for $description")
    }

    private fun selectTab(position: Int) {
        activity {
            val spinner = it.findViewById<Spinner>(R.id.spinner_tabs)
            spinner.setSelection(position)
            spinner.onItemSelectedListener?.onItemSelected(spinner, null, position, position.toLong())
        }
    }

    private fun submit(expression: String) {
        selectTab(0)
        activity {
            it.findViewById<EditText>(R.id.et_input).apply {
                setText(expression)
                onEditorAction(EditorInfo.IME_ACTION_SEND)
            }
        }
        engine { Unit } // Queue barrier: command execution, persistence and its UI callback precede this result.
    }

    private fun output() = activity { it.findViewById<TextView>(R.id.tv_output).text.toString() }

    @Test fun androidJniCorpusAndErrorRecovery() {
        engine {
            for ((expression, expected) in listOf("3!+4!" to 30.0, "(2+1)!*2!" to 12.0,
                "2 xor 3" to 0.0, "4k70" to 4700.0, "4k75" to 4750.0, "1k50" to 1500.0,
                "200+10%" to 220.0, "round(2.65,1)" to 2.7)) {
                assertEquals(expression, "ok", nativeEvaluateDetailed(expression)[0])
                assertEquals(expression, expected, nativeGetLastNumericVal(), 1e-12)
            }
            nativeEvaluate("emulator_mix(a,b)=a+b")
            assertEquals("ok", nativeEvaluateDetailed("emulator_mix([1,2],3)")[0])
            assertArrayEquals(doubleArrayOf(4.0, 5.0), nativeLastArray(), 0.0)
            for (invalid in listOf("[1,]", "1:0:3", "if([1],2,3)", "unknown_call(1)", "1/0")) {
                assertEquals(invalid, "error", nativeEvaluateDetailed(invalid)[0])
                nativeEvaluate("2+2")
                assertEquals(4.0, nativeGetLastNumericVal(), 0.0)
                assertNull(nativeLastArray())
            }
            assertTrue(nativeEvaluate("print(\"Привет 😀\")").contains("Привет 😀"))
            assertEquals("ok", nativeEvaluateDetailed("conv(1, \"km\", \"m\")")[0])
            assertEquals(1000.0, nativeGetLastNumericVal(), 1e-9)
        }
    }

    @Test fun precisionRoundTripAndRotationKeepFullResult() {
        submit("1/7")
        await("initial REPL result") { output().contains("0.142857") }
        activity {
            val spinnerMode = it.findViewById<Spinner>(R.id.spinner_fmt_mode)
            val spinnerPrec = it.findViewById<Spinner>(R.id.spinner_fmt_prec)
            spinnerMode.setSelection(2)
            spinnerPrec.setSelection(0)
            spinnerMode.onItemSelectedListener?.onItemSelected(spinnerMode, null, 2, 2L)
            spinnerPrec.onItemSelectedListener?.onItemSelected(spinnerPrec, null, 0, 0L)
        }
        await("zero precision") { output().contains("= 0\n") }
        activity {
            val spinnerPrec = it.findViewById<Spinner>(R.id.spinner_fmt_prec)
            spinnerPrec.setSelection(12)
            spinnerPrec.onItemSelectedListener?.onItemSelected(spinnerPrec, null, 12, 12L)
        }
        await("restored precision") { output().contains("0.142857142857") }
        val previous = activity { session(it) }
        scenario.recreate()
        await("rotation") { output().contains("0.142857142857") }
        assertSame(previous, activity { session(it) })
    }

    @Test fun scriptDraftRunSaveRenameAndDelete() {
        selectTab(1)
        activity { it.findViewById<View>(R.id.btn_add_item).performClick() }
        await("script editor opened") { activity { session(it).scriptDraft != null } }
        onView(withId(R.id.et_script_name)).perform(replaceText("emulator_case"), closeSoftKeyboard())
        onView(withId(R.id.et_script_code)).perform(replaceText("print(42)"), closeSoftKeyboard())
        scenario.recreate()
        await("script editor restored") { activity { session(it).scriptDraft != null } }
        onView(withId(R.id.et_script_name)).check(matches(withText("emulator_case")))
        onView(withId(R.id.et_script_code)).check(matches(withText("print(42)")))
        onView(withId(R.id.btn_script_run)).perform(click())
        engine { Unit }
        assertFalse(activity { session(it).userScripts.any { script -> script.first == "emulator_case" } })
        onView(withText(R.string.ui_text_015)).perform(click())
        onView(withId(R.id.btn_script_save)).perform(click())
        await("saved script") { activity { session(it).userScripts.any { script -> script.first == "emulator_case" } } }
        assertTrue(engine { nativeEvaluate("run(\"emulator_case\")") }.contains("42"))
        cardAction("emulator_case", R.string.ui_text_009)
        await("script editor opened for rename") { activity { session(it).scriptDraft != null } }
        onView(withId(R.id.et_script_name)).perform(replaceText("fibonacci"), closeSoftKeyboard())
        onView(withId(R.id.btn_script_save)).perform(click())
        assertTrue(activity { session(it).userScripts.any { script -> script.first == "emulator_case" } })
        onView(withId(R.id.et_script_name)).perform(replaceText("emulator_renamed"), closeSoftKeyboard())
        onView(withId(R.id.btn_script_save)).perform(click())
        await("renamed script saved") { activity { session(it).userScripts.any { script -> script.first == "emulator_renamed" } } }
        cardAction("emulator_renamed", R.string.ui_text_010)
        onView(withText(R.string.action_delete)).perform(click())
        assertFalse(activity { session(it).userScripts.any { script -> script.first == "emulator_renamed" } })
    }

    private fun cardAction(name: String, description: Int) {
        activity { screen ->
            val container = screen.findViewById<ViewGroup>(R.id.items_container)
            val card = (0 until container.childCount).map { container.getChildAt(it) }
                .first { it.tag?.toString()?.startsWith("$name ") == true } as ViewGroup
            (0 until card.childCount).map { card.getChildAt(it) }
                .first { it.contentDescription == screen.getString(description) }.performClick()
        }
    }

    @Test fun helpUnitsSearchAndBack() {
        selectTab(7)
        await("Help catalogue") { activity { it.findViewById<ViewGroup>(R.id.items_container).childCount > 0 } }
        activity {
            it.findViewById<View>(R.id.search_toggle).performClick()
            it.findViewById<EditText>(R.id.search_query).setText("xor")
        }
        await("XOR help") { activity { containsLabel(it.findViewById(R.id.items_container), "xor") } }
        activity { it.findViewById<EditText>(R.id.search_query).setText("conv") }
        await("conversion help") { activity { containsLabel(it.findViewById(R.id.items_container), "conv") } }
        onView(withId(R.id.search_query)).perform(closeSoftKeyboard())
        activity { screen ->
            val container = screen.findViewById<ViewGroup>(R.id.items_container)
            (0 until container.childCount).map { container.getChildAt(it) }
                .first { it.tag?.toString()?.startsWith("conv ") == true }.performClick()
        }
        onView(withText(android.R.string.ok)).check(matches(isCompletelyDisplayed())).perform(click())
        pressBack()
        await("exit search") { activity { it.findViewById<View>(R.id.search_query).visibility != View.VISIBLE } }
        selectTab(8)
        for (query in listOf("mm", "millimeter", "milimeter")) {
            activity {
                val search = it.findViewById<EditText>(R.id.search_query)
                if (search.visibility != View.VISIBLE) it.findViewById<View>(R.id.search_toggle).performClick()
                search.setText(query)
            }
            await("Units search $query") { activity { containsLabel(it.findViewById(R.id.items_container), "mm") } }
        }
    }

    private fun containsLabel(view: View, label: String): Boolean =
        view.visibility == View.VISIBLE && ((view is TextView && view.text.toString() == label) ||
            (view is ViewGroup && (0 until view.childCount).any { containsLabel(view.getChildAt(it), label) }))

    @Test fun sessionSurvivesActivityFinishAndRelaunch() {
        submit("retained_value=42")
        submit("retained_array=[1,2,3]")
        submit("retained_formula(x)=x^2")
        submit("fn retained_fn(x) { return x+1 }")
        scenario.close()
        launch()
        engine {
            assertEquals("ok", nativeEvaluateDetailed("retained_value")[0])
            assertEquals(42.0, nativeGetLastNumericVal(), 0.0)
            nativeEvaluate("retained_array")
            assertArrayEquals(doubleArrayOf(1.0, 2.0, 3.0), nativeLastArray(), 0.0)
            nativeEvaluate("retained_formula(3)")
            assertEquals(9.0, nativeGetLastNumericVal(), 0.0)
            nativeEvaluate("retained_fn(3)")
            assertEquals(4.0, nativeGetLastNumericVal(), 0.0)
        }
    }

    @Test fun damagedStorageIsReportedAndNotOverwritten() {
        scenario.close()
        UserDataStorage(context).write("{broken")
        launch()
        assertTrue(activity { session(it).storageLoadFailed })
        submit("2+2")
        assertEquals(4.0, engine { nativeGetLastNumericVal() }, 0.0)
        assertEquals("{broken", UserDataStorage(context).read())
    }

    @Test fun settingsAndHiddenWelcomeSurviveRecreation() {
        assertTrue(output().isEmpty())
        selectTab(6)
        activity {
            it.findViewById<CheckBox>(R.id.btn_settings_enter_action).performClick()
        }
        scenario.recreate()
        assertFalse(activity { it.findViewById<CheckBox>(R.id.btn_settings_enter_action).isChecked })
        assertFalse(context.getSharedPreferences("droidulator_prefs", Context.MODE_PRIVATE).getBoolean("enter_send_mode", true))
    }

    @Test fun plotCoordinatesAndStorageKeepDoublePrecision() {
        val snapshot = engine {
            assertEquals("ok", nativeEvaluateDetailed("plot([1,2],[1234567.89,1234568.89])")[0])
            nativeGetPlotLines() to nativeGetPlotLimits()
        }
        assertEquals(1234567.89, snapshot.first.single().y[0], 0.0)
        val storage = PlotStateStorage(context)
        storage.save(snapshot.first, snapshot.second)
        val restored = CardulatorEngine()
        try {
            restored.initializeOnce()
            await("atomic plot storage") {
                storage.restore(restored)
                restored.nativeGetPlotLines().size == 1
            }
            assertArrayEquals(snapshot.first.single().y, restored.nativeGetPlotLines().single().y, 0.0)
            assertArrayEquals(snapshot.second, restored.nativeGetPlotLimits(), 0.0)
        } finally { restored.close() }
    }

    @Test fun toolbarButtonsClearAndNavigationAndTab() {
        selectTab(0)
        activity {
            it.findViewById<EditText>(R.id.et_input).apply {
                setText("p")
                setSelection(1)
            }
            it.findViewById<View>(R.id.btn_tab).performClick()
        }
        await("tab autocompletion p -> pi") {
            activity { it.findViewById<EditText>(R.id.et_input).text.toString() == "pi" }
        }
        activity {
            it.findViewById<EditText>(R.id.et_input).append("+10")
            it.findViewById<View>(R.id.btn_nav_left).performClick()
        }
        await("cursor moved left") {
            activity { it.findViewById<EditText>(R.id.et_input).selectionStart == 4 }
        }
        activity {
            it.findViewById<View>(R.id.btn_clear).performClick()
        }
        await("input cleared") {
            activity { it.findViewById<EditText>(R.id.et_input).text.isEmpty() }
        }
    }

    @Test fun enterToolbarButtonInNewLineModeAndPlotNavigation() {
        selectTab(6)
        activity {
            it.findViewById<CheckBox>(R.id.btn_settings_enter_action).performClick()
        }
        await("enter send mode disabled") {
            !context.getSharedPreferences("droidulator_prefs", Context.MODE_PRIVATE)
                .getBoolean("enter_send_mode", true)
        }
        selectTab(0)
        await("toolbar enter button visible") {
            activity { it.findViewById<View>(R.id.btn_enter).visibility == View.VISIBLE }
        }
        activity {
            it.findViewById<EditText>(R.id.et_input).setText("plot(sin(x))")
            it.findViewById<View>(R.id.btn_enter).performClick()
        }
        await("plot view visible and tab switched") {
            activity {
                it.findViewById<Spinner>(R.id.spinner_tabs).selectedItemPosition == 5 &&
                    it.findViewById<View>(R.id.plot_view).visibility == View.VISIBLE &&
                    it.findViewById<View>(R.id.toolbar_keyboard).visibility == View.GONE
            }
        }
        // Restore enter mode to send
        selectTab(6)
        activity {
            it.findViewById<CheckBox>(R.id.btn_settings_enter_action).performClick()
        }
    }

    @Test fun variablesAndConstantsCardsDisplay() {
        submit("ui_var_sample=99.5")
        selectTab(4)
        await("variable card visible") {
            activity { containsLabel(it.findViewById(R.id.items_container), "ui_var_sample") }
        }
        selectTab(3)
        await("constants cards visible") {
            activity { it.findViewById<ViewGroup>(R.id.items_container).childCount > 0 }
        }
        selectTab(2)
        await("formulas tab displayed") {
            activity { it.findViewById<View>(R.id.scroll_items).visibility == View.VISIBLE }
        }
    }
}
