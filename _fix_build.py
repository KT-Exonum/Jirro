from pathlib import Path

root = Path(r"C:\Users\jwats\eclipse-workspace\Jirro\Jirro")

# NativeEngine: drop duplicate methods (keep first copies)
ne = root / "app/src/main/java/com/vfxengine/app/NativeEngine.kt"
s = ne.read_text(encoding="utf-8")
marker = "    // Profiling\n    fun getProfileStats(): String {"
first = s.find(marker)
second = s.find(marker, first + 1)
if second != -1:
    end = s.find("    // Crash Recovery", second)
    s = s[:second] + s[end:]
    ne.write_text(s, encoding="utf-8")
    print("NativeEngine: removed duplicate methods")
else:
    print("NativeEngine: no second getProfileStats")

# Common text-field replacement
TEXTFIELD_COLORS = """colors = TextFieldDefaults.colors(
                    focusedTextColor = Color.White,
                    unfocusedTextColor = Color.White,
                    focusedContainerColor = Color(0xFF2D2D2D),
                    unfocusedContainerColor = Color(0xFF1E1E1E),
                    cursorColor = Color.Cyan,
                    focusedIndicatorColor = Color.Cyan,
                    unfocusedIndicatorColor = Color.Transparent,
                    focusedPlaceholderColor = Color.White.copy(alpha = 0.4f),
                    unfocusedPlaceholderColor = Color.White.copy(alpha = 0.4f)
                )"""

replacements_by_file = {}

def patch(rel, mapping):
    p = root / rel
    text = p.read_text(encoding="utf-8")
    orig = text
    for a, b in mapping:
        text = text.replace(a, b)
    if text != orig:
        p.write_text(text, encoding="utf-8")
        print(f"patched {rel}")
    else:
        print(f"no changes {rel}")

# px -> float for graphicsLayer; width uses .dp
px_files = [
    "app/src/main/java/com/vfxengine/app/ui/timeline/Timeline.kt",
    "app/src/main/java/com/vfxengine/app/ui/nodeeditor/NodeEditor.kt",
    "app/src/main/java/com/vfxengine/app/ui/tab/FusionTab.kt",
]
for rel in px_files:
    p = root / rel
    t = p.read_text(encoding="utf-8")
    t2 = t.replace(".px", "")
    t2 = t2.replace("import androidx.compose.ui.unit.px\n", "")
    t2 = t2.replace("import androidx.compose.ui.platform.toPx\n", "")
    t2 = t2.replace("import androidx.compose.ui.unit.toPx\n", "")
    if t2 != t:
        p.write_text(t2, encoding="utf-8")
        print("stripped .px", rel)

# Timeline extras
patch("app/src/main/java/com/vfxengine/app/ui/timeline/Timeline.kt", [
    ("import androidx.compose.ui.graphics.GraphicsLayerScope\n", ""),
    ("import androidx.compose.ui.graphics.Outline\n", ""),
    ("import androidx.compose.ui.graphics.Outline.Generic\n", ""),
    ("import androidx.compose.ui.graphics.Outline.Rectangle\n", ""),
    ("import androidx.compose.foundation.layout.weight\n", ""),
    ("        var markerTime = (visibleStart / markerInterval).ceil() * markerInterval\n",
     "        var markerTime = kotlin.math.ceil(visibleStart / markerInterval) * markerInterval\n"),
    (".width(clipWidth)", ".width(clipWidth.toFloat().dp)"),
])

patch("app/src/main/java/com/vfxengine/app/ui/nodeeditor/NodeEditor.kt", [
    ("import androidx.compose.ui.graphics.GraphicsLayerScope\n", ""),
    ("import androidx.compose.ui.layout.Layout\n", ""),
    ("import androidx.compose.ui.layout.Measurable\n", ""),
    ("import androidx.compose.ui.layout.MeasureResult\n", ""),
    ("import androidx.compose.ui.layout.Placeable\n", ""),
    ("                if (isOutput) translationX = (nodeWidth(node) - 12) else translationX = 0\n",
     "                if (isOutput) translationX = (nodeWidth(node) - 12f) else translationX = 0f\n"),
    ("onDragStart(EditorState.DragState.Connecting(node.id, port.name, 0f, 0f))",
     "onDragStart(EditorState.DragState.Connecting(node.id, port.name, 0f, 0f, isOutput))"),
    ("    val nodeHeight = max(80f, maxPorts * 36f + 40f)\n",
     "    val nodeHeight = maxOf(80f, maxPorts * 36f + 40f)\n"),
])

patch("app/src/main/java/com/vfxengine/app/ui/common/ResizablePanel.kt", [
    ("import androidx.compose.ui.unit.px\n", ""),
])

print("done phase 1")
