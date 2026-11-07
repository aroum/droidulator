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
    private var manualX = false
    private var manualY = false
    private var cachedMinX = -10.0
    private var cachedMaxX = 10.0
    private var cachedMinY = -10.0
    private var cachedMaxY = 10.0
    private var boundsLabelX = ""
    private var boundsLabelY = ""
    private var xTicks = emptyList<String>()
    private var yTicks = emptyList<String>()
    private var xMinManual = 0.0
    private var xMaxManual = 0.0
    private var yMinManual = 0.0
    private var yMaxManual = 0.0

    // Touch inspection state
    private var isTouching = false
    private var touchX = 0f
    private var touchY = 0f
    private data class Probe(val x: Double, val y: Double, val px: Float, val py: Float, val color: Int, val label: String)
    private var currentProbe: Probe? = null

    init {
        updateBounds()
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

    fun updatePlotData(lines: Array<NativePlotLine>, limits: DoubleArray) {
        if (limits.size >= 5) {
            manualX = limits[0] != 0.0
            manualY = if (limits.size >= 6) limits[5] != 0.0 else manualX
            xMinManual = limits[1]
            xMaxManual = limits[2]
            yMinManual = limits[3]
            yMaxManual = limits[4]
        }
        plotLines = lines.map { line ->
            LineData(line.x, line.y, rgb565ToColor(line.color), line.linestyle)
        }
        updateBounds()
        if (isTouching) updateProbe()
        invalidate()
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

    private fun updateBounds() {
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
        if (manualX && xMinManual.isFinite() && xMaxManual.isFinite() && xMinManual < xMaxManual) { minX = xMinManual; maxX = xMaxManual }
        if (manualY && yMinManual.isFinite() && yMaxManual.isFinite() && yMinManual < yMaxManual) { minY = yMinManual; maxY = yMaxManual }
        cachedMinX = minX; cachedMaxX = maxX; cachedMinY = minY; cachedMaxY = maxY
        boundsLabelX = String.format(Locale.ROOT, "X: [%.3g, %.3g]", minX, maxX)
        boundsLabelY = String.format(Locale.ROOT, "Y: [%.3g, %.3g]", minY, maxY)
        xTicks = (0..4).map { String.format(Locale.ROOT, "%.3g", minX + (maxX - minX) * it / 4) }
        yTicks = (0..4).map { String.format(Locale.ROOT, "%.3g", minY + (maxY - minY) * it / 4) }
    }

    private fun updateProbe() {
        val w = width.toFloat(); val h = height.toFloat()
        if (w <= 0f || h <= 0f) { currentProbe = null; return }
        val minX = cachedMinX; val maxX = cachedMaxX
        val minY = cachedMinY; val maxY = cachedMaxY
        val rangeX = maxX - minX; val rangeY = maxY - minY
        val clampedTouchX = touchX.coerceIn(0f, w)
        val valX = minX + (clampedTouchX / w) * rangeX

        // Find closest point on curves
        var closestX = valX
        var closestY = Double.NaN
        var closestDistSq = Double.MAX_VALUE
        var closestPx = clampedTouchX
        var closestPy = touchY.coerceIn(0f, h)
        var pointColor = cursorPointPaint.color

        for (line in plotLines) {
            var prevX = Double.NaN
            var prevY = Double.NaN
            for (i in 0 until minOf(line.x.size, line.y.size)) {
                val xv = line.x[i]
                val yv = line.y[i]
                if (!xv.isFinite() || !yv.isFinite()) { prevX = Double.NaN; prevY = Double.NaN; continue }
                val px = ((xv - minX) / rangeX * w).toFloat()
                val py = (h - ((yv - minY) / rangeY * h)).toFloat()
                var candidateX = xv
                var candidateY = yv
                var candidatePx = px
                var candidatePy = py
                val crossesOutside = (prevY < minY && yv > maxY) || (prevY > maxY && yv < minY) ||
                    (prevY * yv < 0 && abs(yv - prevY) > rangeY * 0.8)
                if (prevX.isFinite() && !crossesOutside) {
                    val ax = ((prevX - minX) / rangeX * w).toFloat()
                    val ay = (h - ((prevY - minY) / rangeY * h)).toFloat()
                    val dx = px - ax; val dy = py - ay
                    val length = dx * dx + dy * dy
                    if (length.isFinite() && length > 0f) {
                        val t = (((touchX - ax) * dx + (touchY - ay) * dy) / length).coerceIn(0f, 1f)
                        candidatePx = ax + t * dx; candidatePy = ay + t * dy
                        candidateX = prevX + (xv - prevX) * t
                        candidateY = prevY + (yv - prevY) * t
                    }
                }
                val distSq = (candidatePx - touchX) * (candidatePx - touchX) + (candidatePy - touchY) * (candidatePy - touchY)
                if (distSq < closestDistSq) {
                    closestDistSq = distSq.toDouble()
                    closestX = candidateX
                    closestY = candidateY
                    closestPx = candidatePx
                    closestPy = candidatePy
                    pointColor = line.color
                }
                prevX = xv; prevY = yv
            }
        }
        val label = if (closestY.isFinite()) String.format(Locale.ROOT, "x: %.3f | y: %.3f", closestX, closestY)
                    else String.format(Locale.ROOT, "x: %.3f", valX)
        currentProbe = Probe(closestX, closestY, closestPx, closestPy, pointColor, label)
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
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
            canvas.drawText("No plot data", (w - textPaint.measureText("No plot data")) / 2f, h / 2f, textPaint)
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
