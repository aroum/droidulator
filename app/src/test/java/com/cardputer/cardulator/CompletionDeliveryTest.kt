package com.cardputer.cardulator

import org.junit.Assert.*
import org.junit.Test

class CompletionDeliveryTest {
    @Test fun cancelSuppressesAnAlreadyQueuedResult() {
        var received = 0
        val delivery = CompletionDelivery<Int> { received += it }
        val queuedResult = { delivery.deliver(7) }
        delivery.cancel()
        queuedResult()
        assertEquals(0, received)
    }

    @Test fun completionIsDeliveredOnlyOnce() {
        var received = 0
        val delivery = CompletionDelivery<Int> { received += it }
        delivery.deliver(3)
        delivery.deliver(5)
        delivery.cancel()
        delivery.deliver(7)
        assertEquals(3, received)
    }
}
