package com.vfxengine.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.weight
import androidx.compose.material3.Card
import androidx.compose.material3.Divider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.vfxengine.app.ui.common.EditorState
import com.vfxengine.app.ui.common.ProfilerOverlay
import com.vfxengine.app.ui.common.ResizablePanel
import com.vfxengine.app.ui.common.SettingsDialog
import com.vfxengine.app.ui.common.Settings
import com.vfxengine.app.ui.tab.DeliverTab
import com.vfxengine.app.ui.tab.EditTab
import com.vfxengine.app.ui.tab.FusionTab
import com.vfxengine.app.ui.tab.MediaTab
import com.vfxengine.app.NativeEngine

/**
 * DaVinci Resolve style 4-tab editor: Media, Edit, Fusion, Deliver
 */
@Composable
fun EditorRoot(engine: com.vfxengine.app.NativeEngine) {
    val state = remember { EditorState(engine) }
    val settings = remember { mutableStateOf(Settings()) }
    var showSettings by remember { mutableStateOf(false) }
    var showProfiler by remember { mutableStateOf(false) }
    var currentTab by remember { mutableStateOf(EditorTab.Media) }
    
    Surface(
        modifier = Modifier.fillMaxSize(),
        color = Color(0xFF0F0F0F)
    ) {
        Box(modifier = Modifier.fillMaxSize()) {
            Column(modifier = Modifier.fillMaxSize()) {
                // Top tab bar (DaVinci style)
                EditorTabBar(
                    currentTab = currentTab,
                    onTabClick = { currentTab = it }
                )
                
                Divider(color = Color.White.copy(alpha = 0.1f))
                
                // Tab content
                when (currentTab) {
                    EditorTab.Media -> MediaTab(state)
                    EditorTab.Edit -> EditTab(state)
                    EditorTab.Fusion -> FusionTab(state)
                    EditorTab.Deliver -> DeliverTab(state, engine)
                }
            }
            
            // Settings dialog overlay
            if (showSettings) {
                SettingsDialog(
                    settings = settings.value,
                    onSave = { newSettings ->
                        settings.value = newSettings
                        applySettingsToEngine(engine, newSettings)
                    },
                    onDismiss = { showSettings = false },
                    nativeEngine = engine
                )
            }
            
            // Profiler overlay
            if (showProfiler) {
                ProfilerOverlay(
                    statsJson = "{}",
                    visible = showProfiler,
                    onDismiss = { showProfiler = false }
                )
            }
        }
    }
}

enum class EditorTab(val label: String, val iconRes: Int) {
    Media("Media", android.R.drawable.ic_menu_gallery),
    Edit("Edit", android.R.drawable.ic_media_play),
    Fusion("Fusion", android.R.drawable.ic_menu_manage),
    Deliver("Deliver", android.R.drawable.ic_media_next)
}

@Composable
fun EditorTabBar(
    currentTab: EditorTab,
    onTabClick: (EditorTab) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(48.dp)
            .background(Color(0xFF121212))
    ) {
        EditorTab.values().forEach { tab ->
            androidx.compose.material3.TextButton(
                onClick = { onTabClick(tab) },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(48.dp),
                colors = androidx.compose.material3.TextButtonDefaults.textButtonColors(
                    containerColor = if (currentTab == tab) Color.Cyan.copy(alpha = 0.1f) else Color.Transparent
                )
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Center,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Icon(
                        painter = painterResource(id = tab.iconRes),
                        contentDescription = "",
                        tint = if (currentTab == tab) Color.Cyan else Color.White.copy(alpha = 0.7f)
                    )
                    androidx.compose.foundation.layout.Box(modifier = Modifier.width(8.dp))
                    Text(
                        text = tab.label,
                        color = if (currentTab == tab) Color.Cyan else Color.White.copy(alpha = 0.9f),
                        fontSize = 13.sp,
                        fontWeight = if (currentTab == tab) FontWeight.Bold else FontWeight.Normal
                    )
                }
            }
        }
        
        // Right side: Settings + Profiler
        Row(horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            androidx.compose.material3.IconButton(onClick = { /* show profiler */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_report_image),
                    contentDescription = "Profiler",
                    tint = Color.White.copy(alpha = 0.7f)
                )
            }
            androidx.compose.material3.IconButton(onClick = { /* show settings */ }) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_preferences),
                    contentDescription = "Settings",
                    tint = Color.White.copy(alpha = 0.7f)
                )
            }
        }
    }
}

private fun applySettingsToEngine(engine: com.vfxengine.app.NativeEngine, settings: Settings) {
    // Apply settings to native engine via JNI commands
}