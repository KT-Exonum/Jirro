from pathlib import Path

root = Path(r"C:\Users\jwats\eclipse-workspace\Jirro\Jirro")

# --- NativeEngine: delete second copies by line numbers ---
ne = root / "app/src/main/java/com/vfxengine/app/NativeEngine.kt"
lines = ne.read_text(encoding="utf-8").splitlines(keepends=True)
# find second 'fun getProfileStats'
hits = [i for i, l in enumerate(lines) if "fun getProfileStats()" in l]
print("getProfileStats lines", [i+1 for i in hits])
if len(hits) >= 2:
    start = hits[1]
    # walk back to comment
    while start > 0 and lines[start-1].strip().startswith("//"):
        start -= 1
        if lines[start].strip() == "":
            break
    # include preceding blank and comment block
    while start > 0 and lines[start-1].strip() in ("",) or (start > 0 and lines[start-1].lstrip().startswith("//")):
        if lines[start-1].lstrip().startswith("//") or lines[start-1].strip() == "":
            start -= 1
        else:
            break
    end = start
    # skip until next method that's not compile/reload or until Crash Recovery
    i = start
    while i < len(lines):
        if i > start and ("fun enableCrashRecovery" in lines[i] or lines[i].strip() == "// Crash Recovery"):
            # include going back to Crash Recovery comment
            while i > start and lines[i].strip() != "// Crash Recovery" and not lines[i].startswith("    // Crash"):
                i -= 1
            end = i
            break
        i += 1
    print("deleting", start+1, "to", end, ":", "".join(lines[start:end])[:200])
    del lines[start:end]
    ne.write_text("".join(lines), encoding="utf-8")

# --- FusionTab header rewrite + mechanical API fixes ---
ft = root / "app/src/main/java/com/vfxengine/app/ui/tab/FusionTab.kt"
text = ft.read_text(encoding="utf-8")

new_imports = '''package com.vfxengine.app.ui.tab

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.RangeSlider
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.vfxengine.app.ui.common.EditorState
import com.vfxengine.app.ui.common.EditorState.KeyframeTrack
import kotlin.math.PI
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt

private fun hsvColor(h: Float, s: Float, v: Float): Color {
    val hue = if (h in 0f..1f && s <= 1f) h * 360f else h
    return Color.hsv(((hue % 360f) + 360f) % 360f, s.coerceIn(0f, 1f), v.coerceIn(0f, 1f))
}

private fun Color.toHsv(): FloatArray {
    val hsv = FloatArray(3)
    android.graphics.Color.colorToHSV(toArgb(), hsv)
    return hsv
}

'''

# drop old package+imports up to first /**
idx = text.find("/**\n * Fusion Tab")
if idx == -1:
    idx = text.find("@Composable\nfun FusionTab")
text = new_imports + text[idx:]

# remove first duplicate FusionTab + FloatingToolToggle: keep from second FusionTab if two exist
count = text.count("fun FusionTab(state: EditorState)")
print("FusionTab count", count)
if count >= 2:
    first = text.find("@Composable\nfun FusionTab(state: EditorState)")
    second = text.find("@Composable\nfun FusionTab(state: EditorState)", first + 1)
    # delete from first through just before second, but keep file-level helpers after first block's closing
    # first block starts at FusionTab and includes FloatingToolToggle before second FusionTab
    text = text[:first] + text[second:]
    print("removed first FusionTab duplicate")

# remove duplicate FloatingToolToggle (keep first remaining)
c2 = text.count("fun FloatingToolToggle")
print("FloatingToolToggle count", c2)
if c2 >= 2:
    f1 = text.find("@Composable\nfun FloatingToolToggle")
    f2 = text.find("@Composable\nfun FloatingToolToggle", f1 + 1)
    # delete second function through its closing brace - find next @Composable after f2+20
    nxt = text.find("\n@Composable", f2 + 20)
    if nxt == -1:
        nxt = text.find("\nfun ", f2 + 20)
    text = text[:f2] + text[nxt+1:]
    print("removed second FloatingToolToggle")

repls = [
    ("androidx.compose.foundation.layout.Divider", "HorizontalDivider"),
    ("androidx.compose.material3.Divider", "HorizontalDivider"),
    ("androidx.compose.material3.FloatingActionButton", "FloatingActionButton"),
    ("androidx.compose.material3.SliderDefaults", "SliderDefaults"),
    ("Color.RGBToHSV(", "("),  # broken - fix next
]
# Do color conversions more carefully after
text = text.replace("androidx.compose.foundation.layout.Divider", "HorizontalDivider")
text = text.replace("androidx.compose.material3.Divider", "HorizontalDivider")
text = text.replace("androidx.compose.material3.FloatingActionButton(", "FloatingActionButton(")
text = text.replace("androidx.compose.material3.SliderDefaults", "SliderDefaults")
text = text.replace("androidx.compose.ui.graphics.Stroke", "Stroke")
text = text.replace("androidx.compose.ui.graphics.StrokeCap", "StrokeCap")
# StrokeCap after Stroke replace might become StrokeCap still if we replaced Stroke inside StrokeCap -> Cap
# Check: "androidx.compose.ui.graphics.StrokeCap" contains "androidx.compose.ui.graphics.Stroke" as prefix!
# So StrokeCap became Cap. Fix Cap -> StrokeCap
text = text.replace("style = Cap.", "style = StrokeCap.")  # unlikely
text = text.replace("cap = Cap.Round", "cap = StrokeCap.Round")
text = text.replace("backgroundColor = Color(0xFF1E1E1E)", "colors = CardDefaults.cardColors(containerColor = Color(0xFF1E1E1E))")

text = text.replace("fun FloatingToolToggle(onClick: () -> Unit)", "fun BoxScope.FloatingToolToggle(onClick: () -> Unit)")
text = text.replace("@Composable\nfun FusionNodeCanvas(state: EditorState)", "@Composable\nfun RowScope.FusionNodeCanvas(state: EditorState)")
text = text.replace("fun QualifierRangeSlider(", "fun RowScope.QualifierRangeSlider(")
text = text.replace("fun MasterSlider(", "fun RowScope.MasterSlider(")

text = text.replace("Color.HSVToColor", "hsvColor")
# RGBToHSV already half-broken: Color.RGBToHSV(x) -> we'll replace remaining
text = text.replace("Color.RGBToHSV", "/*hsv*/")

# RangeSlider API
old_rs = '''        androidx.compose.material3.RangeSlider(
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
'''
# read actual onEndChange block from file after other replacements
text = text.replace("androidx.compose.material3.RangeSlider", "RangeSlider")
text = text.replace("androidx.compose.material3.TextButton", "TextButton")
text = text.replace("androidx.compose.material3.ButtonDefaults", "ButtonDefaults")
text = text.replace("androidx.compose.material3.Card", "Card")

# translationX 0 -> 0f
text = text.replace("translationX = (200f - 16) else translationX = 0", "translationX = (200f - 16f) else translationX = 0f")

# Triple loop
text = text.replace("for ((iq, label) in targets) {", "for ((iqX, iqY, label) in targets) {")
text = text.replace("val x = center.x + iq.first * radius", "val x = center.x + iqX * radius")
text = text.replace("val y = center.y - iq.second * radius", "val y = center.y - iqY * radius")

# drawGrid composable -> DrawScope
text = text.replace("@Composable\nfun drawGrid(width: Float, height: Float)", "fun DrawScope.drawGrid(width: Float, height: Float)")

ft.write_text(text, encoding="utf-8")
print("FusionTab rewritten header, length", len(text))

# Timeline float casts
tl = root / "app/src/main/java/com/vfxengine/app/ui/timeline/Timeline.kt"
t = tl.read_text(encoding="utf-8")
t = t.replace("translationX = x }", "translationX = x.toFloat() }")
t = t.replace("translationX = (x + 2) }", "translationX = (x + 2).toFloat() }")
t = t.replace("translationX = clipStartX", "translationX = clipStartX.toFloat()")
t = t.replace("translationY = 4", "translationY = 4f")
t = t.replace("translationX = playheadX }", "translationX = playheadX.toFloat() }")
t = t.replace("translationX = (playheadX + 4) }", "translationX = (playheadX + 4).toFloat() }")
tl.write_text(t, encoding="utf-8")
print("timeline floats")

print("done")
