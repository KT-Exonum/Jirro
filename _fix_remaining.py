from pathlib import Path
import re

root = Path(r"C:\Users\jwats\eclipse-workspace\Jirro\Jirro\app\src\main\java")

# Timeline: drop duplicate import block (keep first 32 lines + rest from FontWeight)
tl = root / "com/vfxengine/app/ui/timeline/Timeline.kt"
t = tl.read_text(encoding="utf-8")
# keep unique imports by rewriting header until first /**
idx = t.find("/**\n * Timeline ruler")
header = '''package com.vfxengine.app.ui.timeline

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Text
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import com.vfxengine.app.ui.common.EditorState

'''
tl.write_text(header + t[idx:], encoding="utf-8")
print("timeline imports rewritten")

def patch(rel, pairs):
    p = root / rel
    s = p.read_text(encoding="utf-8")
    orig = s
    for a,b in pairs:
        s = s.replace(a,b)
    if s != orig:
        p.write_text(s, encoding="utf-8")
        print("patched", rel)
    else:
        print("unchanged", rel)

# global-ish replacements per file
files = list(root.rglob("*.kt"))
for p in files:
    s = p.read_text(encoding="utf-8")
    o = s
    s = s.replace("Color.Orange", "Color(0xFFFFA726)")
    s = s.replace("androidx.compose.ui.text.TextOverflow", "androidx.compose.ui.text.style.TextOverflow")
    s = s.replace("FontFamily.Mono", "FontFamily.Monospace")
    s = s.replace("android.R.drawable.ic_media_stop", "android.R.drawable.ic_media_pause")
    s = s.replace("android.R.drawable.ic_menu_remove", "android.R.drawable.ic_menu_close_clear_cancel")
    s = s.replace("android.R.drawable.ic_menu_copy", "android.R.drawable.ic_menu_share")
    s = s.replace("android.R.drawable.ic_menu_paste", "android.R.drawable.ic_input_add")
    s = s.replace("SwitchDefaults.colors(thumbColor = Color.Cyan)", "SwitchDefaults.colors(checkedThumbColor = Color.Cyan, checkedTrackColor = Color.Cyan.copy(alpha = 0.5f))")
    if s != o:
        p.write_text(s, encoding="utf-8")
        print("global", p.name)

print("done globals")
