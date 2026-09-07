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
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
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
    
    // Callback to switch tabs
    val onTabSwitch = remember { { tab: EditorTab -> currentTab = tab } }
    
    Surface(
        modifier = Modifier.fillMaxSize(),
        color = Color(0xFF0F0F0F)
    ) {
        Box(modifier = Modifier.fillMaxSize()) {
            Column(modifier = Modifier.fillMaxSize()) {
                // Tab content
                when (currentTab) {
                    EditorTab.Media -> MediaTab(state)
                    EditorTab.Timeline -> EditTab(state, onTabSwitch)
                    EditorTab.Effects -> FusionTab(state)
                    EditorTab.Export -> DeliverTab(state, engine)
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
            
            // Bottom navigation bar
            EditorTabBar(
                currentTab = currentTab,
                onTabClick = { currentTab = it },
                modifier = Modifier.align(Alignment.BottomCenter)
            )
        }
    }
}

enum class EditorTab(val label: String, val iconRes: Int) {
    Media("Media", android.R.drawable.ic_menu_gallery),
    Timeline("Timeline", android.R.drawable.ic_media_play),
    Effects("Effects", android.R.drawable.ic_menu_manage),
    Export("Export", android.R.drawable.ic_media_next)
}

@Composable
fun EditorTabBar(
    currentTab: EditorTab,
    onTabClick: (EditorTab) -> Unit,
    modifier: Modifier = Modifier
) {
    NavigationBar(
        modifier = modifier
            .fillMaxWidth()
            .height(64.dp)
            .background(Color(0xFF121212)),
        containerColor = Color(0xFF121212),
        tonalElevation = 0.dp
    ) {
        EditorTab.values().forEach { tab ->
            NavigationBarItem(
                selected = currentTab == tab,
                onClick = { onTabClick(tab) },
                icon = {
                    Icon(
                        painter = painterResource(id = tab.iconRes),
                        contentDescription = tab.label,
                        tint = if (currentTab == tab) Color.Cyan else Color.White.copy(alpha = 0.7f)
                    )
                },
                label = {
                    Text(
                        text = tab.label,
                        color = if (currentTab == tab) Color.Cyan else Color.White.copy(alpha = 0.7f),
                        fontSize = 11.sp
                    )
                },
                colors = androidx.compose.material3.NavigationBarItemDefaults.colors(
                    selectedIconColor = Color.Cyan,
                    selectedTextColor = Color.Cyan,
                    unselectedIconColor = Color.White.copy(alpha = 0.7f),
                    unselectedTextColor = Color.White.copy(alpha = 0.7f),
                    indicatorColor = Color.Transparent
                )
            )
        }
    }
}

private fun applySettingsToEngine(engine: com.vfxengine.app.NativeEngine, settings: Settings) {
    // Apply settings to native engine via JNI commands
}