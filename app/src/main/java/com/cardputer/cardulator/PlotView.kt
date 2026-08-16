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

class PlotView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    data class LineData(
        val x: DoubleArray,
        val y: DoubleArray,
        val color: Int,
        val linestyle: String
    )

    private var isLightTheme = false

    private val bgPaint = Paint().apply {
        style = Paint.Style.FILL
    }

    private val gridPaint = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = 2f
    }

    private val textPaint = Paint().apply {
        textSize = 28f
        isAntiAlias = true
        typeface = android.graphics.Typeface.MONOSPACE
    }

    private val linePaint = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = 4f
        isAntiAlias = true
    }

    // Touch cursor paints
    private val cursorLinePaint = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = 3f
        pathEffect = DashPathEffect(floatArrayOf(10f, 10f), 0f)
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
        textSize = 30f
        isAntiAlias = true
        typeface = android.graphics.Typeface.MONOSPACE
        isFakeBoldText = true
    }

    private val dashEffect = DashPathEffect(floatArrayOf(15f, 15f), 0f)
    private val dotEffect = DashPathEffect(floatArrayOf(5f, 10f), 0f)
    private val reusablePath = Path()

    private var plotLines = listOf<LineData>()
    private var manualLimits = false
    private var xMinManual = 0.0
    private var xMaxManual = 0.0
    private var yMinManual = 0.0
    private var yMaxManual = 0.0

    // Touch inspection state
    private var isTouching = false
    private var touchX = 0f
    private var touchY = 0f

    init {
        updateThemeColors()
    }

    fun setThemeMode(isLight: Boolean) {
        if (isLightTheme != isLight) {
            isLightTheme = isLight
            updateThemeColors()
            invalidate()
        }
    }

    private fun updateThemeColors() {
        if (isLightTheme) {
            bgPaint.color = Color.parseColor("#FBF1C7")        // Gruvbox Light BG
            gridPaint.color = Color.parseColor("#D5C4A1")      // Gruvbox Light Gray
            textPaint.color = Color.parseColor("#3C3836")      // Gruvbox Dark FG
            cursorLinePaint.color = Color.parseColor("#928374") // Gruvbox Gray
            cursorPointPaint.color = Color.parseColor("#9D0006")// Gruvbox Red
            cursorTextBgPaint.color = Color.parseColor("#EBDBB2")// Gruvbox Light Soft BG
            cursorTextPaint.color = Color.parseColor("#282828") // Gruvbox Dark
        } else {
            bgPaint.color = Color.parseColor("#1D2021")        // Gruvbox Dark BG
            gridPaint.color = Color.parseColor("#504945")      // Gruvbox Dark Gray
            textPaint.color = Color.parseColor("#EBDBB2")      // Gruvbox FG
            cursorLinePaint.color = Color.parseColor("#A89984") // Gruvbox Gray
            cursorPointPaint.color = Color.parseColor("#FB4934")// Gruvbox Red
            cursorTextBgPaint.color = Color.parseColor("#3C3836")// Gruvbox Soft BG
            cursorTextPaint.color = Color.parseColor("#FBF1C7") // Gruvbox Light
        }
    }

    fun updatePlotData(rawPlotData: String) {
        val lines = rawPlotData.trim().split("\n")
        if (lines.size < 3) {
            plotLines = emptyList()
            invalidate()
            return
        }

        try {
            val count = lines[0].trim().toIntOrNull() ?: 0
            manualLimits = lines[1].trim() == "1"
            val lims = lines[2].trim().split(" ")
            if (lims.size >= 4) {
                xMinManual = lims[0].toDoubleOrNull() ?: 0.0
                xMaxManual = lims[1].toDoubleOrNull() ?: 0.0
                yMinManual = lims[2].toDoubleOrNull() ?: 0.0
                yMaxManual = lims[3].toDoubleOrNull() ?: 0.0
            }

            val parsedLines = mutableListOf<LineData>()
            var idx = 3
            for (i in 0 until count) {
                if (idx + 2 >= lines.size) break
                val meta = lines[idx].trim().split(";")
                val colorRgb565 = meta.getOrNull(0)?.toIntOrNull() ?: 0x07E0
                val linestyle = meta.getOrNull(1) ?: "-"
                val size = meta.getOrNull(2)?.toIntOrNull() ?: 0

                val xStr = lines[idx + 1].trim().split(" ")
                val yStr = lines[idx + 2].trim().split(" ")

                val xArr = DoubleArray(size) { j -> xStr.getOrNull(j)?.toDoubleOrNull() ?: Double.NaN }
                val yArr = DoubleArray(size) { j -> yStr.getOrNull(j)?.toDoubleOrNull() ?: Double.NaN }

                val androidColor = rgb565ToColor(colorRgb565)
                parsedLines.add(LineData(xArr, yArr, androidColor, linestyle))
                idx += 3
            }
            plotLines = parsedLines
        } catch (e: Exception) {
            e.printStackTrace()
            plotLines = emptyList()
        }
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
                invalidate()
                return true
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                isTouching = false
                invalidate()
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val w = width.toFloat()
        val h = height.toFloat()

        canvas.drawRect(0f, 0f, w, h, bgPaint)

        var minX = Double.MAX_VALUE
        var maxX = -Double.MAX_VALUE
        var minY = Double.MAX_VALUE
        var maxY = -Double.MAX_VALUE

        if (manualLimits) {
            minX = xMinManual
            maxX = xMaxManual
            minY = yMinManual
            maxY = yMaxManual
        } else {
            var hasPoints = false
            for (line in plotLines) {
                for (i in line.x.indices) {
                    val xv = line.x[i]
                    val yv = line.y[i]
                    if (xv.isNaN() || !xv.isFinite() || yv.isNaN() || !yv.isFinite()) continue
                    if (xv < minX) minX = xv
                    if (xv > maxX) maxX = xv
                    if (yv < minY) minY = yv
                    if (yv > maxY) maxY = yv
                    hasPoints = true
                }
            }
            if (!hasPoints) {
                minX = -10.0; maxX = 10.0
                minY = -10.0; maxY = 10.0
            } else {
                var dx = maxX - minX
                if (dx == 0.0) dx = 1.0
                minX -= dx * 0.05
                maxX += dx * 0.05

                var dy = maxY - minY
                if (dy == 0.0) dy = 1.0
                minY -= dy * 0.05
                maxY += dy * 0.05
            }
        }

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

        // Draw labels
        canvas.drawText(String.format("X: [%.1f, %.1f]", minX, maxX), 20f, 40f, textPaint)
        canvas.drawText(String.format("Y: [%.1f, %.1f]", minY, maxY), 20f, 80f, textPaint)

        if (plotLines.isEmpty()) {
            canvas.drawText("No plot data", w / 2 - 80f, h / 2, textPaint)
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

            for (i in line.x.indices) {
                val xv = line.x[i]
                val yv = line.y[i]
                if (xv.isNaN() || !xv.isFinite() || yv.isNaN() || !yv.isFinite()) {
                    starting = true
                    continue
                }
                val px = ((xv - minX) / rangeX * w).toFloat()
                val py = (h - ((yv - minY) / rangeY * h)).toFloat()

                if (starting) {
                    reusablePath.moveTo(px, py)
                    starting = false
                } else {
                    reusablePath.lineTo(px, py)
                }
            }
            canvas.drawPath(reusablePath, linePaint)
        }

        // Draw interactive touch cursor
        if (isTouching) {
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
                for (i in line.x.indices) {
                    val xv = line.x[i]
                    val yv = line.y[i]
                    if (xv.isNaN() || !xv.isFinite() || yv.isNaN() || !yv.isFinite()) continue
                    val px = ((xv - minX) / rangeX * w).toFloat()
                    val py = (h - ((yv - minY) / rangeY * h)).toFloat()

                    val distSq = (px - touchX) * (px - touchX) + (py - touchY) * (py - touchY)
                    if (distSq < closestDistSq) {
                        closestDistSq = distSq.toDouble()
                        closestX = xv
                        closestY = yv
                        closestPx = px
                        closestPy = py
                        pointColor = line.color
                    }
                }
            }

            // Draw crosshairs
            canvas.drawLine(closestPx, 0f, closestPx, h, cursorLinePaint)
            canvas.drawLine(0f, closestPy, w, closestPy, cursorLinePaint)

            // Draw highlighted point on curve
            cursorPointPaint.color = pointColor
            canvas.drawCircle(closestPx, closestPy, 10f, cursorPointPaint)

            // Draw inspection tooltip banner
            val infoText = if (!closestY.isNaN()) {
                String.format("x: %.3f | y: %.3f", closestX, closestY)
            } else {
                String.format("x: %.3f", valX)
            }

            val textWidth = cursorTextPaint.measureText(infoText)
            val boxPadding = 16f
            val boxW = textWidth + boxPadding * 2
            val boxH = 50f

            var boxLeft = closestPx + 20f
            if (boxLeft + boxW > w - 10f) {
                boxLeft = closestPx - boxW - 20f
            }
            if (boxLeft < 10f) boxLeft = 10f

            var boxTop = closestPy - 60f
            if (boxTop < 10f) {
                boxTop = closestPy + 20f
            }

            canvas.drawRect(boxLeft, boxTop, boxLeft + boxW, boxTop + boxH, cursorTextBgPaint)
            canvas.drawText(infoText, boxLeft + boxPadding, boxTop + 35f, cursorTextPaint)
        }
    }
}
