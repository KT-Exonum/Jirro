from pathlib import Path

p = Path(r"C:\Users\jwats\eclipse-workspace\Jirro\Jirro\app\src\main\java\com\vfxengine\app\ui\tab\FusionTab.kt")
text = p.read_text(encoding="utf-8")

# Fix broken imports
text = text.replace("import ButtonDefaults\n", "import androidx.compose.material3.ButtonDefaults\n")
text = text.replace("import Card\n", "import androidx.compose.material3.Card\n")
text = text.replace("import CardDefaults\n", "import androidx.compose.material3.CardDefaults\n")
text = text.replace("import RangeSlider\n", "import androidx.compose.material3.RangeSlider\n")
text = text.replace("import SliderDefaults\n", "import androidx.compose.material3.SliderDefaults\n")
text = text.replace("import TextButton\n", "import androidx.compose.material3.TextButton\n")
text = text.replace("import StrokeCap\n", "import androidx.compose.ui.graphics.StrokeCap\n")

if "import androidx.compose.ui.graphics.graphicsLayer" not in text:
    text = text.replace(
        "import androidx.compose.ui.graphics.Color\n",
        "import androidx.compose.ui.graphics.Color\nimport androidx.compose.ui.graphics.graphicsLayer\n",
    )

text = text.replace("/*hsv*/(currentColor)", "currentColor.toHsv()")
text = text.replace("/*hsv*/(color)", "color.toHsv()")
text = text.replace("hsv.value[0] * 360f", "hsv.value[0]")
text = text.replace("hsv[0] * 360f", "hsv[0]")

old_rs = """        RangeSlider(
            modifier = Modifier.fillMaxWidth(),
            start = rangeStart,
            end = rangeEnd,
            onStartChange = { v ->
                rangeStart = v
                state.updateNodeUniform(node.id, "${uniformBase}Start", v)
            },
            onEndChange = { v ->
                rangeEnd = v
                state.updateNodeUniform(node.id, "${uniformBase}End", v)
            },
            rangeStart = min,
            rangeEnd = max,
            colors = SliderDefaults.colors(
                thumbColor = Color.Cyan,
                activeTrackColor = Color.Cyan,
                inactiveTrackColor = Color.White.copy(alpha = 0.1f)
            )
        )"""
new_rs = """        RangeSlider(
            modifier = Modifier.fillMaxWidth(),
            value = rangeStart..rangeEnd,
            onValueChange = { r ->
                rangeStart = r.start
                rangeEnd = r.endInclusive
                state.updateNodeUniform(node.id, "${uniformBase}Start", r.start)
                state.updateNodeUniform(node.id, "${uniformBase}End", r.endInclusive)
            },
            valueRange = min..max,
            colors = SliderDefaults.colors(
                thumbColor = Color.Cyan,
                activeTrackColor = Color.Cyan,
                inactiveTrackColor = Color.White.copy(alpha = 0.1f)
            )
        )"""
if old_rs not in text:
    print("RangeSlider block not found exact")
    # show nearby
    i = text.find("RangeSlider(")
    print(repr(text[i:i+700]))
else:
    text = text.replace(old_rs, new_rs)
    print("replaced RangeSlider")

text = text.replace(
    "@Composable\nfun RowScope.QualifierRangeSlider(",
    "@OptIn(ExperimentalMaterial3Api::class)\n@Composable\nfun RowScope.QualifierRangeSlider(",
)

text = text.replace("for (i in 0..radius step 2)", "for (i in 0..radius.toInt() step 2)")

# drawText -> native canvas
old_dt = """            drawText(
                text = label,
                color = Color.White,
                fontSize = 10.sp,
                topLeft = Offset(x + 10.dp.toPx(), y - 10.dp.toPx())
            )"""
new_dt = """            drawContext.canvas.nativeCanvas.drawText(
                label,
                x + 10.dp.toPx(),
                y - 10.dp.toPx(),
                android.graphics.Paint().apply {
                    color = android.graphics.Color.WHITE
                    textSize = 10.sp.toPx()
                    isAntiAlias = true
                }
            )"""
text = text.replace(old_dt, new_dt)

# Parade labels
old_dt2 = """            drawText(
                text = listOf("R", "G", "B")[ch],
                color = listOf(Color.Red, Color.Green, Color.Blue)[ch],
                fontSize = 12.sp,
                topLeft = Offset(4f, top + 4f)
            )"""
new_dt2 = """            val chColor = listOf(Color.Red, Color.Green, Color.Blue)[ch]
            drawContext.canvas.nativeCanvas.drawText(
                listOf("R", "G", "B")[ch],
                4f,
                top + 16f,
                android.graphics.Paint().apply {
                    color = chColor.toArgb()
                    textSize = 12.sp.toPx()
                    isAntiAlias = true
                }
            )"""
if old_dt2 not in text:
    print("parade drawText not found")
    i = text.find('listOf("R", "G", "B")')
    print(repr(text[i-80:i+250]))
else:
    text = text.replace(old_dt2, new_dt2)
    print("replaced parade drawText")

p.write_text(text, encoding="utf-8")
print("wrote fusiontab")
