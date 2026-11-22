package com.cardputer.cardulator

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.DashPathEffect
import android.graphics.Paint
import android.graphics.Path
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import kotlin.math.abs
import java.util.Locale
import java.util.concurrent.Executors
import java.util.concurrent.Future

class PlotView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    class LineData(
        val x: DoubleArray,
        val y: DoubleArray,
        val color: Int,
        val linestyle: String
    )

    private val density = resources.displayMetrics.density
    private val scaledDensity = resources.displayMetrics.scaledDensity
    private fun dp(value: Float) = value * density
    private var isLightTheme = false

    private val bgPaint = Paint().apply {
        style = Paint.Style.FILL
    }

    private val gridPaint = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = dp(1f)
    }

    private val textPaint = Paint().apply {
        textSize = 12f * scaledDensity
        isAntiAlias = true
        typeface = android.graphics.Typeface.MONOSPACE
    }

    private val linePaint = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = dp(2f)
        isAntiAlias = true
    }

    // Touch cursor paints
    private val cursorLinePaint = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = dp(1.5f)
        pathEffect = DashPathEffect(floatArrayOf(dp(5f), dp(5f)), 0f)
        isAntiAlias = true
    }

    private val cursorPointPaint = Paint().apply {
        style = Paint.Style.FILL
        isAntiAlias = true
    }

    private val cursorTextBgPaint = Paint().apply {
        style = Paint.Style.FILL
    }

    private val cursorTextPaint = Paint().apply {
        textSize = 13f * scaledDensity
        isAntiAlias = true
        typeface = android.graphics.Typeface.MONOSPACE
        isFakeBoldText = true
    }

    private val dashEffect = DashPathEffect(floatArrayOf(dp(8f), dp(8f)), 0f)
    private val dotEffect = DashPathEffect(floatArrayOf(dp(3f), dp(5f)), 0f)
    private val reusablePath = Path()

    private var plotLines = listOf<LineData>()
    private var bounds = Bounds(-10.0, 10.0, -10.0, 10.0)
    private val cachedMinX get() = bounds.minX
    private val cachedMaxX get() = bounds.maxX
    private val cachedMinY get() = bounds.minY
    private val cachedMaxY get() = bounds.maxY
    private val boundsLabelX get() = bounds.labelX
    private val boundsLabelY get() = bounds.labelY
    private val xTicks get() = bounds.xTicks
    private val yTicks get() = bounds.yTicks

    // Touch inspection state
    private var isTouching = false
    private var touchX = 0f
    private var touchY = 0f
    private data class Probe(val x: Double, val y: Double, val px: Float, val py: Float, val color: Int, val label: String)
    private var currentProbe: Probe? = null
    private data class ProbeSegment(val x0: Double, val y0: Double, val x1: Double, val y1: Double,
                                    val px0: Float, val py0: Float, val px1: Float, val py1: Float, val color: Int)
    private data class ProbeIndex(
        val segments: List<ProbeSegment> = emptyList(),
        val bins: List<List<Int>> = List(64) { emptyList() },
        val wideSegments: List<Int> = emptyList(),
        val visits: IntArray = IntArray(segments.size)
    )
    private var probeIndex = ProbeIndex()
    private var pendingProbeIndex: Future<*>? = null
    private var indexRequest = 0L
    private var probeGeneration = 0

    init {
        contentDescription = context.getString(R.string.plot_accessibility)
        isFocusable = true
        updateThemeColors()
    }

    fun setThemeMode(isLight: Boolean) {
        isLightTheme = isLight
        updateThemeColors()
        invalidate()
    }

    private fun updateThemeColors() {
        bgPaint.color = ThemePalette.bg
        gridPaint.color = ThemePalette.borderColor
        textPaint.color = ThemePalette.fgText
        cursorLinePaint.color = ThemePalette.fgDim
        cursorPointPaint.color = ThemePalette.red
        cursorTextBgPaint.color = ThemePalette.bgSoft
        cursorTextPaint.color = ThemePalette.fgText
    }

    internal fun updatePlotData(data: PlotData) {
        plotLines = data.lines
        bounds = data.bounds
        rebuildProbeIndex()
        if (isTouching) updateProbe()
        invalidate()
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE -> {
                isTouching = true
                touchX = event.x
                touchY = event.y
                updateProbe()
                invalidate()
                return true
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (event.actionMasked == MotionEvent.ACTION_UP) performClick()
                isTouching = false
                invalidate()
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }

    internal data class Bounds(val minX: Double, val maxX: Double, val minY: Double, val maxY: Double) {
        val labelX = String.format(Locale.ROOT, "X: [%.3g, %.3g]", minX, maxX)
        val labelY = String.format(Locale.ROOT, "Y: [%.3g, %.3g]", minY, maxY)
        val xTicks = (0..4).map { String.format(Locale.ROOT, "%.3g", minX + (maxX - minX) * it / 4) }
        val yTicks = (0..4).map { String.format(Locale.ROOT, "%.3g", minY + (maxY - minY) * it / 4) }
    }

    internal data class PlotData(val lines: List<LineData>, val bounds: Bounds)

    companion object {
        private val probeExecutor = Executors.newSingleThreadExecutor()

        internal fun preparePlotData(lines: Array<NativePlotLine>, limits: DoubleArray): PlotData {
            val plotLines = lines.map { LineData(it.x, it.y, rgb565ToColor(it.color), it.linestyle) }
            var minX = Double.POSITIVE_INFINITY
            var maxX = Double.NEGATIVE_INFINITY
            var minY = Double.POSITIVE_INFINITY
            var maxY = Double.NEGATIVE_INFINITY
            for (line in plotLines) {
                for (i in 0 until minOf(line.x.size, line.y.size)) {
                    val x = line.x[i]; val y = line.y[i]
                    if (!x.isFinite() || !y.isFinite()) continue
                    minX = minOf(minX, x); maxX = maxOf(maxX, x)
                    minY = minOf(minY, y); maxY = maxOf(maxY, y)
                }
            }
            if (!minX.isFinite()) { minX = -10.0; maxX = 10.0; minY = -10.0; maxY = 10.0 }
            else {
                val dx = (maxX - minX).takeIf { it.isFinite() && it > 0 } ?: 1.0
                val dy = (maxY - minY).takeIf { it.isFinite() && it > 0 } ?: 1.0
                minX -= dx * .05; maxX += dx * .05; minY -= dy * .05; maxY += dy * .05
            }
            if (limits.size >= 5) {
                val manualX = limits[0] != 0.0
                val manualY = if (limits.size >= 6) limits[5] != 0.0 else manualX
                if (manualX && limits[1].isFinite() && limits[2].isFinite() && limits[1] < limits[2]) {
                    minX = limits[1]
                    maxX = limits[2]
                }
                if (manualY && limits[3].isFinite() && limits[4].isFinite() && limits[3] < limits[4]) {
                    minY = limits[3]
                    maxY = limits[4]
                }
            }
            return PlotData(plotLines, Bounds(minX, maxX, minY, maxY))
        }

        private fun rgb565ToColor(rgb565: Int): Int {
            val r5 = (rgb565 shr 11) and 0x1F
            val g6 = (rgb565 shr 5) and 0x3F
            val b5 = rgb565 and 0x1F
            val r = (r5 * 255) / 31
            val g = (g6 * 255) / 63
            val b = (b5 * 255) / 31
            return Color.rgb(r, g, b)
        }
    }

    private fun rebuildProbeIndex() {
        pendingProbeIndex?.cancel(true)
        val request = ++indexRequest
        probeIndex = ProbeIndex()
        probeGeneration = 0
        val w = width.toFloat()
        val h = height.toFloat()
        if (!isAttachedToWindow || w <= 0f || h <= 0f || plotLines.isEmpty()) return
        val lines = plotLines
        val plotBounds = bounds
        val main = android.os.Handler(android.os.Looper.getMainLooper())
        pendingProbeIndex = probeExecutor.submit {
            val index = buildProbeIndex(lines, plotBounds, w, h)
            if (!Thread.currentThread().isInterrupted) main.post {
                if (request != indexRequest) return@post
                probeIndex = index
                if (isTouching) updateProbe()
                invalidate()
            }
        }
    }

    private fun buildProbeIndex(plotLines: List<LineData>, bounds: Bounds, w: Float, h: Float): ProbeIndex {
        val rangeX = bounds.maxX - bounds.minX
        val rangeY = bounds.maxY - bounds.minY
        if (!rangeX.isFinite() || !rangeY.isFinite() || rangeX <= 0 || rangeY <= 0) return ProbeIndex()
        val probeSegments = ArrayList<ProbeSegment>()
        val probeBins = List(64) { ArrayList<Int>() }
        val wideProbeSegments = ArrayList<Int>()
        fun add(x0: Double, y0: Double, x1: Double, y1: Double, color: Int) {
            val px0 = ((x0 - bounds.minX) / rangeX * w).toFloat()
            val py0 = (h - (y0 - bounds.minY) / rangeY * h).toFloat()
            val px1 = ((x1 - bounds.minX) / rangeX * w).toFloat()
            val py1 = (h - (y1 - bounds.minY) / rangeY * h).toFloat()
            if (!px0.isFinite() || !py0.isFinite() || !px1.isFinite() || !py1.isFinite()) return
            val index = probeSegments.size
            probeSegments.add(ProbeSegment(x0, y0, x1, y1, px0, py0, px1, py1, color))
            val first = (minOf(px0, px1) / w * 64).toInt().coerceIn(0, 63)
            val last = (maxOf(px0, px1) / w * 64).toInt().coerceIn(0, 63)
            if (last - first > 8) wideProbeSegments.add(index)
            else for (bin in first..last) probeBins[bin].add(index)
        }
        for (line in plotLines) {
            var prevX = Double.NaN; var prevY = Double.NaN
            for (i in 0 until minOf(line.x.size, line.y.size)) {
                if (Thread.currentThread().isInterrupted) return ProbeIndex()
                val x = line.x[i]; val y = line.y[i]
                if (!x.isFinite() || !y.isFinite()) { prevX = Double.NaN; prevY = Double.NaN; continue }
                val discontinuity = (prevY < bounds.minY && y > bounds.maxY) || (prevY > bounds.maxY && y < bounds.minY) ||
                    (prevY * y < 0 && abs(y - prevY) > rangeY * 0.8)
                if (prevX.isFinite() && !discontinuity) add(prevX, prevY, x, y, line.color)
                else add(x, y, x, y, line.color)
                prevX = x; prevY = y
            }
        }
        return ProbeIndex(probeSegments, probeBins, wideProbeSegments)
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        rebuildProbeIndex()
    }

    override fun onDetachedFromWindow() {
        ++indexRequest
        pendingProbeIndex?.cancel(true)
        pendingProbeIndex = null
        super.onDetachedFromWindow()
    }

    private fun updateProbe() {
        val w = width.toFloat(); val h = height.toFloat()
        if (w <= 0f || h <= 0f) { currentProbe = null; return }
        val clampedX = touchX.coerceIn(0f, w)
        val valX = cachedMinX + clampedX / w * (cachedMaxX - cachedMinX)
        var closestX = valX; var closestY = Double.NaN
        var closestPx = clampedX; var closestPy = touchY.coerceIn(0f, h)
        var pointColor = cursorPointPaint.color
        var best = Double.POSITIVE_INFINITY
        if (probeGeneration == Int.MAX_VALUE) { probeIndex.visits.fill(0); probeGeneration = 0 }
        val generation = ++probeGeneration
        fun inspect(index: Int) {
            if (probeIndex.visits[index] == generation) return
            probeIndex.visits[index] = generation
            val segment = probeIndex.segments[index]
            val dx = segment.px1 - segment.px0; val dy = segment.py1 - segment.py0
            val length = dx * dx + dy * dy
            val t = if (length.isFinite() && length > 0f)
                (((touchX - segment.px0) * dx + (touchY - segment.py0) * dy) / length).coerceIn(0f, 1f) else 0f
            val px = segment.px0 + t * dx; val py = segment.py0 + t * dy
            val distance = (px - touchX).toDouble() * (px - touchX) + (py - touchY).toDouble() * (py - touchY)
            if (distance < best) {
                best = distance; closestPx = px; closestPy = py
                closestX = segment.x0 + (segment.x1 - segment.x0) * t
                closestY = segment.y0 + (segment.y1 - segment.y0) * t
                pointColor = segment.color
            }
        }
        probeIndex.wideSegments.forEach(::inspect)
        val center = (clampedX / w * 64).toInt().coerceIn(0, 63)
        for (radius in 0..63) {
            val left = center - radius; val right = center + radius
            if (left >= 0) probeIndex.bins[left].forEach(::inspect)
            if (radius > 0 && right < 64) probeIndex.bins[right].forEach(::inspect)
            val lower = if (left > 0) (clampedX - left * w / 64).coerceAtLeast(0f).toDouble() else Double.POSITIVE_INFINITY
            val upper = if (right < 63) ((right + 1) * w / 64 - clampedX).coerceAtLeast(0f).toDouble() else Double.POSITIVE_INFINITY
            val nearestUnsearched = minOf(lower, upper)
            if (nearestUnsearched * nearestUnsearched > best || (left <= 0 && right >= 63)) break
        }
        val label = if (closestY.isFinite()) String.format(Locale.ROOT, "x: %.3f | y: %.3f", closestX, closestY)
                    else String.format(Locale.ROOT, "x: %.3f", valX)
        currentProbe = Probe(closestX, closestY, closestPx, closestPy, pointColor, label)
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        rebuildProbeIndex()
        if (isTouching) updateProbe()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val w = width.toFloat()
        val h = height.toFloat()

        canvas.drawRect(0f, 0f, w, h, bgPaint)

        val minX = cachedMinX
        val maxX = cachedMaxX
        val minY = cachedMinY
        val maxY = cachedMaxY
        if (w <= 0f || h <= 0f) return

        val rangeX = if (maxX - minX == 0.0) 1.0 else maxX - minX
        val rangeY = if (maxY - minY == 0.0) 1.0 else maxY - minY

        // Draw axes
        val cx = ((0.0 - minX) / rangeX * w).toFloat()
        val cy = (h - ((0.0 - minY) / rangeY * h)).toFloat()

        if (cx in 0f..w) {
            canvas.drawLine(cx, 0f, cx, h, gridPaint)
        }
        if (cy in 0f..h) {
            canvas.drawLine(0f, cy, w, cy, gridPaint)
        }

        val xAxis = cy.coerceIn(0f, h)
        val yAxis = cx.coerceIn(0f, w)
        for (i in 0..4) {
            val x = w * i / 4f
            val y = h - h * i / 4f
            canvas.drawLine(x, xAxis - dp(3f), x, xAxis + dp(3f), gridPaint)
            canvas.drawLine(yAxis - dp(3f), y, yAxis + dp(3f), y, gridPaint)
            val xLabel = xTicks[i]
            canvas.drawText(xLabel, (x - textPaint.measureText(xLabel) / 2f).coerceIn(0f, (w - textPaint.measureText(xLabel)).coerceAtLeast(0f)), (xAxis + dp(14f)).coerceAtMost(h - dp(2f)), textPaint)
            canvas.drawText(yTicks[i], (yAxis + dp(5f)).coerceAtMost((w - textPaint.measureText(yTicks[i])).coerceAtLeast(0f)), (y - dp(3f)).coerceAtLeast(dp(10f)), textPaint)
        }

        // Draw labels
        canvas.drawText(boundsLabelX, dp(8f), dp(18f), textPaint)
        canvas.drawText(boundsLabelY, dp(8f), dp(36f), textPaint)

        if (plotLines.isEmpty()) {
            val emptyMessage = context.getString(R.string.plot_empty)
            canvas.drawText(emptyMessage, (w - textPaint.measureText(emptyMessage)) / 2f, h / 2f, textPaint)
            return
        }

        // Draw lines
        for (line in plotLines) {
            linePaint.color = line.color
            when (line.linestyle) {
                "--" -> linePaint.pathEffect = dashEffect
                ":", "-." -> linePaint.pathEffect = dotEffect
                else -> linePaint.pathEffect = null
            }

            reusablePath.reset()
            var starting = true
            var previousY = 0.0

            for (i in line.x.indices) {
                val xv = line.x[i]
                val yv = line.y[i]
                if (xv.isNaN() || !xv.isFinite() || yv.isNaN() || !yv.isFinite()) {
                    starting = true
                    continue
                }
                val px = ((xv - minX) / rangeX * w).toFloat()
                val py = (h - ((yv - minY) / rangeY * h)).toFloat()

                val crossesOutside = !starting && (((previousY < minY && yv > maxY) || (previousY > maxY && yv < minY)) ||
                    (previousY * yv < 0 && abs(yv - previousY) > rangeY * 0.8))
                if (starting || crossesOutside) {
                    reusablePath.moveTo(px, py)
                    starting = false
                } else {
                    reusablePath.lineTo(px, py)
                }
                previousY = yv
            }
            canvas.drawPath(reusablePath, linePaint)
        }

        // Draw interactive touch cursor
        if (isTouching) {
            val probe = currentProbe ?: return
            val closestPx = probe.px
            val closestPy = probe.py
            val pointColor = probe.color
            val infoText = probe.label

            // Draw crosshairs
            canvas.drawLine(closestPx, 0f, closestPx, h, cursorLinePaint)
            canvas.drawLine(0f, closestPy, w, closestPy, cursorLinePaint)

            // Draw highlighted point on curve
            cursorPointPaint.color = pointColor
            canvas.drawCircle(closestPx, closestPy, dp(5f), cursorPointPaint)

            // Draw inspection tooltip banner
            val textWidth = cursorTextPaint.measureText(infoText)
            val boxPadding = dp(8f)
            val boxW = textWidth + boxPadding * 2
            val boxH = cursorTextPaint.fontMetrics.let { it.descent - it.ascent } + dp(12f)

            var boxLeft = closestPx + dp(10f)
            if (boxLeft + boxW > w - dp(5f)) {
                boxLeft = closestPx - boxW - dp(10f)
            }
            if (boxLeft < dp(5f)) boxLeft = dp(5f)

            var boxTop = closestPy - boxH - dp(5f)
            if (boxTop < dp(5f)) {
                boxTop = closestPy + dp(10f)
            }

            canvas.drawRect(boxLeft, boxTop, boxLeft + boxW, boxTop + boxH, cursorTextBgPaint)
            canvas.drawText(infoText, boxLeft + boxPadding, boxTop + dp(6f) - cursorTextPaint.fontMetrics.ascent, cursorTextPaint)
        }
    }
}
