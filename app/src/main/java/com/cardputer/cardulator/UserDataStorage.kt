package com.cardputer.cardulator

import android.content.Context
import android.util.AtomicFile
import java.io.File
import java.io.FileNotFoundException

/** Stores the user's session with recovery from an interrupted write. */
class UserDataStorage(context: Context) {
    private val file = AtomicFile(File(context.filesDir, "user_data.json"))

    fun read(): String? = try {
        file.openRead().use { it.readBytes().toString(Charsets.UTF_8) }
    } catch (_: FileNotFoundException) {
        null
    }

    fun write(data: String) {
        val stream = file.startWrite()
        try {
            stream.write(data.toByteArray(Charsets.UTF_8))
            file.finishWrite(stream)
        } catch (error: Exception) {
            file.failWrite(stream)
            throw error
        }
    }
}
