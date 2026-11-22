package com.cardputer.cardulator.benchmarks

import android.os.Build
import android.os.SystemClock
import android.view.KeyEvent
import androidx.benchmark.macro.CompilationMode
import androidx.benchmark.macro.ExperimentalMetricApi
import androidx.benchmark.macro.FrameTimingMetric
import androidx.benchmark.macro.StartupMode
import androidx.benchmark.macro.StartupTimingMetric
import androidx.benchmark.macro.TraceSectionMetric
import androidx.benchmark.macro.junit4.MacrobenchmarkRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Direction
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObject2
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/** End-to-end timings include automation overhead. App trace sections isolate app work. */
@RunWith(AndroidJUnit4::class)
@OptIn(ExperimentalMetricApi::class)
class PerformanceBenchmark {
    @get:Rule val benchmark = MacrobenchmarkRule()
    private val arguments get() = InstrumentationRegistry.getArguments()
    private val device get() = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
    private val iterations get() = (arguments.getString("performanceIterations") ?: "3").toInt().also {
        require(it in 1..10)
    }

    @Before fun resetIsolatedBenchmarkApp() {
        check(arguments.getString("ephemeralDevice") == "true")
        check(Build.HARDWARE in listOf("ranchu", "goldfish") || Build.FINGERPRINT.contains("generic"))
        // This package belongs only to the benchmark variant, never the installed user app.
        check(device.executeShellCommand("pm clear $PACKAGE").trim() == "Success")
    }

    private fun view(id: String): UiObject2 =
        device.wait(Until.findObject(By.res(PACKAGE, id)), TIMEOUT)
            ?: failWithWindow("Timed out finding $id")

    private fun failWithWindow(message: String): Nothing {
        // Macrobenchmark stops the target on failure, so capture its window before throwing.
        runCatching {
            val window = java.io.ByteArrayOutputStream()
            device.dumpWindowHierarchy(window)
            window.toString("UTF-8").chunked(3500).forEach {
                android.util.Log.e("PerformanceBenchmark", it)
            }
        }
        error(message)
    }

    private fun await(description: String, condition: () -> Boolean) {
        val deadline = SystemClock.uptimeMillis() + TIMEOUT
        while (SystemClock.uptimeMillis() < deadline) {
            if (condition()) return
            SystemClock.sleep(50)
        }
        failWithWindow("Timed out waiting for $description")
    }

    private fun ready() {
        view("et_input")
        view("spinner_tabs")
    }

    private fun choose(id: String, label: String) {
        val spinner = view(id)
        if (spinner.findObject(By.text(label)) != null) return
        spinner.click()
        if (!device.hasObject(By.text(label))) {
            check(UiScrollable(UiSelector().className("android.widget.ListView"))
                .scrollTextIntoView(label)) { "Missing dropdown item $label" }
        }
        checkNotNull(device.wait(Until.findObject(By.text(label)), TIMEOUT)).click()
        await("$id selection $label") { view(id).findObject(By.text(label)) != null }
    }

    private fun output(): String = view("tv_output").text.orEmpty()
    private fun normalizedOutput() = output().filterNot(Char::isWhitespace)

    private fun prepareRepl() {
        choose("spinner_tabs", "Settings")
        val sendOnEnter = view("btn_settings_enter_action")
        if (sendOnEnter.isChecked) sendOnEnter.click()
        await("toolbar send enabled") { !view("btn_settings_enter_action").isChecked }
        choose("spinner_tabs", "REPL")
        ready()
        view("btn_enter")
    }

    private fun submit(expression: String, plot: Boolean = false) {
        val previous = output()
        // A hardware Enter is intercepted by Gboard on the hosted emulator and can
        // be decoded asynchronously against an old input connection. Submit through
        // the real toolbar instead; keyboard correctness belongs to regression tests.
        view("et_input").text = expression
        await("input populated") { view("et_input").text == expression }
        view("btn_enter").click()
        if (plot) {
            view("plot_view")
            device.waitForIdle()
        } else {
            await("result of $expression") {
                val text = output()
                text != previous && text.contains("> $expression")
            }
            check(!device.hasObject(By.res(PACKAGE, "tv_error_overlay"))) {
                "Workload returned an error: $expression"
            }
        }
    }

    private fun seedHistory() {
        for (i in 1..60) submit("$i/7")
        device.waitForIdle()
    }

    @Test fun coldStartup() = benchmark.measureRepeated(
        packageName = PACKAGE,
        metrics = listOf(StartupTimingMetric()),
        compilationMode = CompilationMode.Full(),
        startupMode = StartupMode.COLD,
        iterations = iterations,
        setupBlock = { pressHome() }
    ) {
        startActivityAndWait()
        ready()
    }

    @Test fun historyScroll() {
        var seeded = false
        benchmark.measureRepeated(
            packageName = PACKAGE,
            metrics = listOf(FrameTimingMetric()),
            compilationMode = CompilationMode.Full(),
            iterations = iterations,
            setupBlock = {
                if (!seeded) {
                    startActivityAndWait()
                    ready()
                    prepareRepl()
                    seedHistory()
                    seeded = true
                }
            }
        ) {
            val history = view("scroll_view")
            repeat(3) {
                history.swipe(Direction.DOWN, 0.8f)
                history.swipe(Direction.UP, 0.8f)
            }
            device.waitForIdle()
        }
    }

    @Test fun historyPrecision() {
        var seeded = false
        benchmark.measureRepeated(
            packageName = PACKAGE,
            metrics = listOf(FrameTimingMetric(), TraceSectionMetric("Droidulator.replRebuild")),
            compilationMode = CompilationMode.Full(),
            iterations = iterations,
            setupBlock = {
                if (!seeded) {
                    startActivityAndWait()
                    ready()
                    prepareRepl()
                    seedHistory()
                    choose("spinner_fmt_mode", "FIX")
                    choose("spinner_fmt_prec", "12dp")
                    await("initial 12 digits") { normalizedOutput().contains("8.57142857142") }
                    seeded = true
                }
            }
        ) {
            choose("spinner_fmt_prec", "0dp")
            await("zero digits") { !normalizedOutput().contains("8.571428") }
            choose("spinner_fmt_prec", "12dp")
            await("restored 12 digits") { normalizedOutput().contains("8.57142857142") }
            device.waitForIdle()
        }
    }

    @Test fun largeScriptTyping() = benchmark.measureRepeated(
        packageName = PACKAGE,
        metrics = listOf(FrameTimingMetric(), TraceSectionMetric("Droidulator.highlight")),
        compilationMode = CompilationMode.Full(),
        iterations = iterations,
        setupBlock = {
            if (device.hasObject(By.res(PACKAGE, "et_script_code"))) {
                view("btn_script_cancel").click()
            }
            startActivityAndWait()
            choose("spinner_tabs", "Scripts")
            view("btn_add_item").click()
            view("et_script_code").apply {
                text = (1..500).joinToString("\n") { "value_$it = sin($it) + $it # sample $it" } + "\nvalue = "
                click()
            }
            device.pressKeyCode(KeyEvent.KEYCODE_MOVE_END, KeyEvent.META_CTRL_ON)
            device.waitForIdle()
        }
    ) {
        repeat(6) { device.pressKeyCode(KeyEvent.KEYCODE_1) }
        repeat(6) { device.pressKeyCode(KeyEvent.KEYCODE_DEL) }
        device.waitForIdle()
    }

    @Test fun heavyPlot() = benchmark.measureRepeated(
        packageName = PACKAGE,
        metrics = listOf(FrameTimingMetric(), TraceSectionMetric("Droidulator.command")),
        compilationMode = CompilationMode.Full(),
        iterations = iterations,
        setupBlock = {
            startActivityAndWait()
            choose("spinner_tabs", "REPL")
            ready()
            prepareRepl()
        }
    ) {
        submit("plot((0:1:10000),sin((0:1:10000)))", plot = true)
        val plot = view("plot_view")
        repeat(3) {
            plot.swipe(Direction.LEFT, 0.5f)
            plot.swipe(Direction.RIGHT, 0.5f)
        }
        device.waitForIdle()
    }

    @Test fun largeArrayCalculation() = benchmark.measureRepeated(
        packageName = PACKAGE,
        metrics = listOf(FrameTimingMetric(), TraceSectionMetric("Droidulator.command")),
        compilationMode = CompilationMode.Full(),
        iterations = iterations,
        setupBlock = { startActivityAndWait(); ready(); prepareRepl() }
    ) {
        submit("sum(sqrt((1:1:20000)))")
        device.waitForIdle()
    }

    private companion object {
        const val PACKAGE = "com.cardputer.cardulator.benchmark"
        const val TIMEOUT = 30_000L
    }
}
