package com.cardputer.cardulator

import android.os.Handler
import android.os.Looper
import androidx.lifecycle.ViewModel
import androidx.lifecycle.MutableLiveData
import java.util.concurrent.Executors
import java.util.concurrent.Future

data class ScriptDraft(
    val originalName: String?, var name: String, var code: String,
    var nameCursor: Int = name.length, var codeCursor: Int = code.length
)

data class DefinitionDraft(
    val title: String, val hint: String, val tab: String,
    var expression: String, var cursor: Int = expression.length
)

data class FormulaRunDraft(val formula: NativeFormula, val arguments: List<String>)

class CalculatorSession : ViewModel() {
    private val engineDelegate = lazy { CardulatorEngine() }
    val engine by engineDelegate
    private val engineQueue = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())

    fun <T> compute(operation: CardulatorEngine.() -> T, completed: (Result<T>) -> Unit): Future<*> =
        engineQueue.submit {
            val result = runCatching { engine.operation() }
            if (!Thread.currentThread().isInterrupted) main.post { completed(result) }
        }

    data class CommandResult(
        val entry: ReplDisplayItem, val showPlot: Boolean, val scriptName: String?,
        val definition: DefinitionDraft? = null
    )
    internal val commandResult = MutableLiveData<CommandResult?>(null)
    internal var commandRunning = false
        private set

    fun runCommand(
        input: String, scriptName: String? = null, definition: DefinitionDraft? = null,
        persist: CardulatorEngine.() -> Unit
    ): Boolean {
        if (commandRunning) return false
        commandRunning = true
        compute({
            val entry = if (scriptName == null) evaluateEntry(input) else
                ReplDisplayItem("", nativeRunScript(input), false, isRawText = true)
            if (!entry.isError) persist()
            entry
        }) { result ->
            commandRunning = false
            val entry = result.getOrElse {
                ReplDisplayItem(input, it.localizedMessage.orEmpty(), true)
            }
            if (scriptName == null && definition == null && replHistory.lastOrNull() != input) {
                replHistory.add(input)
                if (replHistory.size > 500) replHistory.removeAt(0)
            }
            val showPlot = !entry.isError && scriptName == null && definition == null &&
                (input.startsWith("plot(") || input.startsWith("plot."))
            if (!entry.isError && definition != null && definitionDraft === definition) definitionDraft = null
            commandResult.value = CommandResult(entry, showPlot, scriptName, definition)
        }
        return true
    }

    override fun onCleared() {
        engineQueue.execute { if (engineDelegate.isInitialized()) engine.close() }
        engineQueue.shutdown()
    }

    var definitionDraft: DefinitionDraft? = null
    var formulaRunDraft: FormulaRunDraft? = null
    var scriptDraft: ScriptDraft? = null
    var initialized = false
    var storageLoadFailed = false
    val userScripts = mutableListOf(
        Pair("fibonacci", "x = 1\ny = 1\nfor (i = 1; i <= 10; i++) {\n  print(x)\n  tmp = x + y\n  x = y\n  y = tmp\n}"),
        Pair("factorial_test", "n = 5\nres = 1\nfor (i = 1; i <= n; i++) {\n  res = res * i\n}\nprint(res)")
    )

    val replHistory = mutableListOf<String>()
    val replDisplayItems = mutableListOf<ReplDisplayItem>()
}
