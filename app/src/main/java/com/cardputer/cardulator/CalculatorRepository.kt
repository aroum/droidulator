package com.cardputer.cardulator

import android.content.Context

internal class CalculatorRepository(context: Context, private val session: CalculatorSession) {
    private val storage = UserDataStorage(context)
    private val userScripts get() = session.userScripts

    fun save() {
        try {
            if (session.storageLoadFailed) return
            val json = org.json.JSONObject()

            val variables = session.engine.nativeValues(false)
                .filterNot { it.name.matches(Regex("e[0-9]+")) }
                .map { "${it.name} = ${it.expressionValue()}" }
            json.put("variables", org.json.JSONArray(variables))
            json.put("formulas", org.json.JSONArray(session.engine.nativeFormulas().toList()))
            val constants = session.engine.nativeValues(true)
                .filterNot { it.name == "pi" || it.name == "e" }
                .map { "${it.name} = ${it.expressionValue()}" }
            json.put("constants", org.json.JSONArray(constants))

            val functionsArray = org.json.JSONArray()
            for (function in session.engine.nativeGetScriptFunctions()) {
                val fields = org.json.JSONArray()
                function.forEach { fields.put(it) }
                functionsArray.put(fields)
            }
            json.put("scriptFunctions", functionsArray)

            // Save scripts
            val scriptsArray = org.json.JSONArray()
            for (s in userScripts) {
                val scriptObj = org.json.JSONObject()
                scriptObj.put("name", s.first)
                scriptObj.put("code", s.second)
                scriptsArray.put(scriptObj)
            }
            json.put("scripts", scriptsArray)

            storage.write(json.toString())
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun parseStoredNumber(text: String): Double = when (text.trim().lowercase()) {
        "nan", "+nan", "-nan" -> Double.NaN
        "inf", "+inf", "infinity", "+infinity" -> Double.POSITIVE_INFINITY
        "-inf", "-infinity" -> Double.NEGATIVE_INFINITY
        else -> text.trim().toDouble()
    }

    private fun restoreStoredVariable(line: String, isConstant: Boolean) {
        val parts = line.split("=", limit = 2)
        require(parts.size == 2) { "Invalid saved variable" }
        val name = parts[0].trim()
        val value = parts[1].trim()
        if (!isConstant && value.startsWith("[") && value.endsWith("]")) {
            val body = value.substring(1, value.length - 1).trim()
            val values = if (body.isEmpty()) doubleArrayOf() else body.split(",").map(::parseStoredNumber).toDoubleArray()
            session.engine.nativeRestoreArray(name, values)
        } else {
            session.engine.nativeRestoreScalar(name, parseStoredNumber(value), isConstant)
        }
    }

    fun load(restoreEngine: Boolean, onError: () -> Unit) {
        try {
            val contents = storage.read() ?: return
            val json = org.json.JSONObject(contents)

            val functions = json.optJSONArray("scriptFunctions")
            if (restoreEngine && functions != null) {
                for (i in 0 until functions.length()) {
                    val fields = functions.getJSONArray(i)
                    session.engine.nativeRestoreScriptFunction(Array(fields.length()) { fields.getString(it) })
                }
            }

            // Restore variables
            if (restoreEngine && json.has("variables")) {
                val varsArray = json.getJSONArray("variables")
                for (i in 0 until varsArray.length()) {
                    val line = varsArray.getString(i)
                    restoreStoredVariable(line, false)
                }
            }

            // Restore formulas
            if (restoreEngine && json.has("formulas")) {
                val formulasArray = json.getJSONArray("formulas")
                for (i in 0 until formulasArray.length()) {
                    val line = formulasArray.getString(i)
                    session.engine.nativeEvaluate(line)
                }
            }

            // Restore constants
            if (restoreEngine && json.has("constants")) {
                val constsArray = json.getJSONArray("constants")
                for (i in 0 until constsArray.length()) {
                    val line = constsArray.getString(i)
                    restoreStoredVariable(line, true)
                }
            }

            // Restore scripts
            if (json.has("scripts")) {
                val scriptsArray = json.getJSONArray("scripts")
                userScripts.clear()
                for (i in 0 until scriptsArray.length()) {
                    val obj = scriptsArray.getJSONObject(i)
                    val originalName = obj.getString("name")
                    var name = originalName
                    var suffix = 2
                    while (userScripts.any { it.first == name }) name = "${originalName}_${suffix++}"
                    val code = obj.getString("code")
                    userScripts.add(Pair(name, code))
                    // Register each script so run(name) can find it
                    session.engine.nativeRegisterScript(name, code)
                }
            }
        } catch (e: Exception) {
            session.storageLoadFailed = true
            android.util.Log.e("Droidulator", "Unable to load saved data", e)
            onError()
        }
    }
}
