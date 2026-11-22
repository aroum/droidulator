package com.cardputer.cardulator

/** Sections are no-ops unless a system trace is collecting this application's events. */
internal inline fun <T> performanceTrace(name: String, block: () -> T): T {
    android.os.Trace.beginSection(name)
    return try { block() } finally { android.os.Trace.endSection() }
}
