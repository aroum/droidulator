package com.cardputer.cardulator

import android.content.Context
import android.util.AtomicFile
import android.util.Log
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.File
import java.io.FileNotFoundException
import java.util.concurrent.Executors

/** Saves plot snapshots separately from the text session, without blocking the UI on disk I/O. */
class PlotStateStorage(context: Context) {
    private val file = AtomicFile(File(context.applicationContext.filesDir, "plot_state.bin"))

    companion object {
        private const val MAGIC = 0x44504c54
        private const val VERSION = 1
        private const val MAX_POINTS = 100_000
        private val writer = Executors.newSingleThreadExecutor()
    }

    fun save(lines: Array<NativePlotLine>, limits: DoubleArray) {
        writer.execute {
            var stream: java.io.FileOutputStream? = null
            try {
                require(lines.size <= 4 && limits.size == 7)
                stream = file.startWrite()
                val out = DataOutputStream(BufferedOutputStream(stream))
                out.writeInt(MAGIC); out.writeInt(VERSION); out.writeInt(lines.size)
                for (line in lines) {
                    require(line.x.size == line.y.size && line.x.size <= MAX_POINTS && line.linestyle.length <= 32)
                    out.writeInt(line.x.size); out.writeInt(line.color); out.writeUTF(line.linestyle)
                    for (v in line.x) out.writeDouble(v)
                    for (v in line.y) out.writeDouble(v)
                }
                for (limit in limits) out.writeDouble(limit)
                out.flush()
                file.finishWrite(stream)
            } catch (error: Exception) {
                if (stream != null) file.failWrite(stream)
                Log.e("Droidulator", "Unable to save plot", error)
            }
        }
    }

    fun restore() {
        try {
            val (lines, limits) = DataInputStream(BufferedInputStream(file.openRead())).use { input ->
                require(input.readInt() == MAGIC && input.readInt() == VERSION)
                val count = input.readInt(); require(count in 0..4)
                val lines = Array(count) {
                    val size = input.readInt(); require(size in 0..MAX_POINTS)
                    val color = input.readInt(); val style = input.readUTF(); require(style.length <= 32)
                    val x = DoubleArray(size) { input.readDouble() }
                    val y = DoubleArray(size) { input.readDouble() }
                    NativePlotLine(x, y, color, style)
                }
                lines to DoubleArray(7) { input.readDouble() }
            }
            CardulatorEngine.nativeRestorePlot(lines, limits)
        } catch (_: FileNotFoundException) {
            // No saved plot yet.
        } catch (error: Exception) {
            Log.e("Droidulator", "Unable to restore plot", error)
        }
    }
}
