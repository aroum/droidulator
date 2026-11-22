package com.cardputer.cardulator

import android.content.Context
import android.os.Handler
import android.os.Looper
import java.util.concurrent.Executors

internal object UnitsDatabase {
    private val worker = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())
    @Volatile private var ready = false

    /** Called on the main thread. Completion is always delivered there. */
    fun initialize(context: Context, complete: (Throwable?) -> Unit): CompletionDelivery<Throwable?> {
        val delivery = CompletionDelivery(complete)
        val application = context.applicationContext
        if (ready) {
            delivery.deliver(null)
            return delivery
        }
        worker.execute {
            val failure = try {
                if (!ready) {
                    CardulatorEngine.nativeInitUnits(installUnitsData(application))
                    ready = true
                }
                null
            } catch (error: Exception) {
                error
            } catch (error: UnsatisfiedLinkError) {
                error
            }
            main.post { delivery.deliver(failure) }
        }
        return delivery
    }

    private fun installUnitsData(context: Context): String {
        // Keep each upstream database version separate so app updates replace old data.
        val unitsDir = java.io.File(context.filesDir, "gnu-units-2.27")
        check(unitsDir.isDirectory || unitsDir.mkdirs()) { "Unable to create units database directory" }
        for (name in listOf("currency.units", "crypto.units", "metal_prices.units", "cpi.units", "elements.units", "units.dat")) {
            val destination = android.util.AtomicFile(java.io.File(unitsDir, name))
            val expected = context.assets.open(name).use { it.readBytes() }
            val current = try { destination.openRead().use { it.readBytes() } }
                catch (_: java.io.FileNotFoundException) { null }
            if (current == null || !current.contentEquals(expected)) {
                val stream = destination.startWrite()
                try {
                    stream.write(expected)
                    destination.finishWrite(stream)
                } catch (error: Exception) {
                    destination.failWrite(stream)
                    throw error
                }
            }
        }
        return java.io.File(unitsDir, "units.dat").absolutePath
    }
}
