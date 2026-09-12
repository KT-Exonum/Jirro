package com.vfxengine.app.ui

import android.net.Uri
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
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Card
import androidx.compose.material3.Divider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.ui.text.style.TextOverflow
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
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
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
import org.json.JSONArray
import org.json.JSONObject

/**
 * DaVinci Resolve style 4-tab editor: Media, Edit, Fusion, Deliver
 */
@Composable
fun EditorRoot(
    engine: com.vfxengine.app.NativeEngine,
    pickMedia: (Uri) -> Unit,
    pickProject: (Uri) -> Unit,
    saveProjectAs: (Uri) -> Unit
) {
    val state = remember { EditorState(engine) }
    val settings = remember { mutableStateOf(Settings()) }
    var showSettings by remember { mutableStateOf(false) }
    var showProfiler by remember { mutableStateOf(false) }
    var currentTab by remember { mutableStateOf(EditorTab.Media) }
    var showWelcome by remember { mutableStateOf(true) }
    var showNewProjectDialog by remember { mutableStateOf(false) }
    var showOpenProjectDialog by remember { mutableStateOf(false) }
    var newProjectName by remember { mutableStateOf("Untitled Project") }
    
    // Check if project is loaded
    val hasProject by remember { mutableStateOf(engine.hasProject()) }
    
    // Callback to switch tabs
    val onTabSwitch = remember { { tab: EditorTab -> currentTab = tab } }
    
    // Update welcome screen visibility
    if (hasProject && showWelcome) {
        showWelcome = false
    }
    
    // Save As callback - triggers file picker
    val onSaveProjectAs = remember { { 
        saveProjectAs(Uri.parse("project.vfxproj"))
    } }

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = Color(0xFF0F0F0F)
    ) {
        Box(modifier = Modifier.fillMaxSize()) {
            Column(modifier = Modifier.fillMaxSize()) {
                // Tab content
                when (currentTab) {
                    EditorTab.Media -> MediaTab(state, { pickMedia(android.net.Uri.EMPTY) })
                    EditorTab.Timeline -> EditTab(state, onTabSwitch)
                    EditorTab.Effects -> FusionTab(state)
                    EditorTab.Export -> DeliverTab(state, engine)
                }
            }
            
            // Welcome screen (shows when no project is open)
            if (showWelcome) {
                WelcomeScreen(
                    engine = engine,
                    onNewProject = { showNewProjectDialog = true },
                    onOpenProject = { 
                        pickProject(android.net.Uri.EMPTY)
                    },
                    onRecentProjectClick = { path ->
                        engine.openProject(path)
                        showWelcome = false
                    }
                )
            }
            
            // New Project Dialog
            if (showNewProjectDialog) {
                NewProjectDialog(
                    projectName = newProjectName,
                    onNameChange = { newProjectName = it },
                    onCreate = { name ->
                        engine.newProject(name)
                        showNewProjectDialog = false
                        showWelcome = false
                    },
                    onDismiss = { showNewProjectDialog = false }
                )
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
                    nativeEngine = engine,
                    onSaveProjectAs = onSaveProjectAs
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
            
            // Bottom navigation bar (hidden on welcome screen)
            if (!showWelcome) {
                EditorTabBar(
                    currentTab = currentTab,
                    onTabClick = { currentTab = it },
                    modifier = Modifier.align(Alignment.BottomCenter)
                )
            }
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

/**
 * Welcome Screen - Shows on first launch when no project is open
 */
@Composable
fun WelcomeScreen(
    engine: com.vfxengine.app.NativeEngine,
    onNewProject: () -> Unit,
    onOpenProject: () -> Unit,
    onRecentProjectClick: (String) -> Unit
) {
    val recentProjects = remember { mutableStateOf<List<RecentProject>>(emptyList()) }
    
    // Load recent projects
    androidx.compose.runtime.LaunchedEffect(Unit) {
        val json = engine.getRecentProjects()
        try {
            val array = JSONArray(json)
            val projects = mutableListOf<RecentProject>()
            for (i in 0 until array.length()) {
                val obj = array.getJSONObject(i)
                projects.add(RecentProject(
                    name = obj.getString("name"),
                    path = obj.getString("path"),
                    lastOpened = obj.getString("lastOpened")
                ))
            }
            recentProjects.value = projects
        } catch (e: Exception) {
            // Ignore parse errors
        }
    }
    
    Box(
        modifier = Modifier.fillMaxSize(),
        contentAlignment = Alignment.Center
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(32.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(24.dp)
        ) {
            // App logo/title
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
            ) {
                Icon(
                    painter = painterResource(id = android.R.drawable.ic_menu_gallery),
                    contentDescription = "VFX Engine",
                    tint = Color.Cyan,
                    modifier = Modifier.size(64.dp)
                )
                Text(
                    text = "VFX Engine",
                    color = Color.White,
                    fontSize = 32.sp,
                    fontWeight = FontWeight.Bold
                )
                Text(
                    text = "Professional Mobile Video Editor",
                    color = Color.White.copy(alpha = 0.6f),
                    fontSize = 14.sp
                )
            }
            
            androidx.compose.foundation.layout.Box(modifier = Modifier.height(16.dp))
            
            // Primary actions
            Column(
                modifier = Modifier.fillMaxWidth(),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(12.dp)
            ) {
                Button(
                    onClick = onNewProject,
                    modifier = Modifier.fillMaxWidth().height(56.dp),
                    colors = androidx.compose.material3.ButtonDefaults.buttonColors(containerColor = Color.Cyan)
                ) {
                    Row(
                        modifier = Modifier.fillMaxSize(),
                        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Center,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(
                            painter = painterResource(id = android.R.drawable.ic_menu_add),
                            contentDescription = "New",
                            tint = Color.Black,
                            modifier = Modifier.size(24.dp)
                        )
                        androidx.compose.foundation.layout.Box(modifier = Modifier.width(12.dp))
                        Text(text = "New Project", color = Color.Black, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                    }
                }
                
                OutlinedButton(
                    onClick = onOpenProject,
                    modifier = Modifier.fillMaxWidth().height(56.dp),
                    colors = androidx.compose.material3.ButtonDefaults.outlinedButtonColors(
                        contentColor = Color.Cyan
                    )
                ) {
                    Row(
                        modifier = Modifier.fillMaxSize(),
                        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Center,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(
                            painter = painterResource(id = android.R.drawable.ic_menu_upload),
                            contentDescription = "Open",
                            tint = Color.Cyan,
                            modifier = Modifier.size(24.dp)
                        )
                        androidx.compose.foundation.layout.Box(modifier = Modifier.width(12.dp))
                        Text(text = "Open Project", color = Color.Cyan, fontSize = 16.sp, fontWeight = FontWeight.Medium)
                    }
                }
            }
            
            // Recent projects
            if (recentProjects.value.isNotEmpty()) {
                Column(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
                ) {
                    Text(text = "Recent Projects", color = Color.White.copy(alpha = 0.7f), fontSize = 14.sp, fontWeight = FontWeight.Medium)
                    Column(
                        modifier = Modifier.fillMaxWidth(),
                        verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
                    ) {
                        recentProjects.value.forEach { project ->
                            RecentProjectCard(
                                project = project,
                                onClick = { onRecentProjectClick(project.path) }
                            )
                        }
                    }
                }
            }
        }
    }
}

data class RecentProject(
    val name: String,
    val path: String,
    val lastOpened: String
)

@Composable
fun RecentProjectCard(
    project: RecentProject,
    onClick: () -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp),
        onClick = onClick,
        colors = androidx.compose.material3.CardDefaults.cardColors(
            containerColor = Color(0xFF1E1E1E)
        )
    ) {
        Row(
            modifier = Modifier.fillMaxSize().padding(16.dp),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(4.dp)
            ) {
                Text(text = project.name, color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                Text(text = project.path, color = Color.White.copy(alpha = 0.5f), fontSize = 11.sp, maxLines = 1, overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis)
                Text(text = "Last opened: ${project.lastOpened}", color = Color.White.copy(alpha = 0.4f), fontSize = 10.sp)
            }
            Icon(
                painter = painterResource(id = android.R.drawable.ic_media_play),
                contentDescription = "Open",
                tint = Color.Cyan,
                modifier = Modifier.size(24.dp)
            )
        }
    }
}

/**
 * New Project Dialog
 */
@Composable
fun NewProjectDialog(
    projectName: String,
    onNameChange: (String) -> Unit,
    onCreate: (String) -> Unit,
    onDismiss: () -> Unit
) {
    Box(
        modifier = Modifier.fillMaxSize(),
        contentAlignment = Alignment.Center
    ) {
        // Scrim
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black.copy(alpha = 0.5f))
                .fillMaxSize(),
            contentAlignment = Alignment.Center
        ) {
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(24.dp)
                    .background(Color(0xFF1E1E1E))
            ) {
                Column(
                    modifier = Modifier.padding(24.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
                ) {
                    Text(text = "New Project", color = Color.White, fontSize = 20.sp, fontWeight = FontWeight.Bold)
                    
                    TextField(
                        value = projectName,
                        onValueChange = onNameChange,
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        textStyle = androidx.compose.ui.text.TextStyle(color = Color.White, fontSize = 16.sp),
                        colors = TextFieldDefaults.colors(
                            focusedTextColor = Color.White,
                            unfocusedTextColor = Color.White,
                            focusedContainerColor = Color(0xFF2D2D2D),
                            unfocusedContainerColor = Color(0xFF121212),
                            cursorColor = Color.Cyan
                        ),
                        placeholder = { Text(text = "Project Name", color = Color.White.copy(alpha = 0.4f), fontSize = 16.sp) },
                        leadingIcon = {
                            Icon(
                                painter = painterResource(id = android.R.drawable.ic_menu_edit),
                                contentDescription = "Project Name",
                                tint = Color.White.copy(alpha = 0.5f)
                            )
                        }
                    )
                    
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.End,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        TextButton(onClick = onDismiss) {
                            Text(text = "Cancel", color = Color.White.copy(alpha = 0.7f), fontSize = 14.sp)
                        }
                        androidx.compose.foundation.layout.Box(modifier = Modifier.width(8.dp))
                        Button(
                            onClick = { onCreate(projectName) },
                            colors = androidx.compose.material3.ButtonDefaults.buttonColors(containerColor = Color.Cyan)
                        ) {
                            Text(text = "Create", color = Color.Black, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            }
        }
    }
}