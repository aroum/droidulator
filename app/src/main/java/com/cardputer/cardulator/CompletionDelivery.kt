package com.cardputer.cardulator

import java.util.concurrent.atomic.AtomicReference

/** Cancellation releases the callback even after work has queued its result. */
internal class CompletionDelivery<T>(callback: (T) -> Unit) {
    private val callback = AtomicReference<((T) -> Unit)?>(callback)
    fun deliver(value: T) { callback.getAndSet(null)?.invoke(value) }
    fun cancel() { callback.set(null) }
}
