package com.cardputer.cardulator

import android.os.Handler
import android.os.Looper
import androidx.lifecycle.ViewModel
import java.util.concurrent.Executors
import java.util.concurrent.Future

data class ScriptDraft(val originalName: String?, var name: String, var code: String)

data class DefinitionDraft(
    val title: String, val hint: String, val tab: String,
    var expression: String, var cursor: Int = expression.length
)

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

    override fun onCleared() {
        engineQueue.execute { if (engineDelegate.isInitialized()) engine.close() }
        engineQueue.shutdown()
    }

    var definitionDraft: DefinitionDraft? = null
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
