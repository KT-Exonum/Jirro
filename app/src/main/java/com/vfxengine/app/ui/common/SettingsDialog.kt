package com.vfxengine.app.ui.common

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.Checkbox
import androidx.compose.material3.Divider
import androidx.compose.material3.FilledTextField
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt
import com.vfxengine.app.NativeEngine

/**
 * Settings data class
 */
data class Settings(
    var useVulkan: Boolean = true,
    var maxConcurrentDecoders: Int = 4,
    var frameCacheSizeMB: Int = 256,
    var enableHdr: Boolean = false,
    var defaultFrameRate: Double = 30.0,
    var enableThermalThrottling: Boolean = true,
    var logLevel: LogLevel = LogLevel.Info,
    var autoSaveIntervalSec: Int = 60,
    var uiScale: Float = 1.0f,
    var theme: Theme = Theme.Dark
) {
    enum class LogLevel(val label: String) { Error("Error"), Warn("Warn"), Info("Info"), Debug("Debug") }
    enum class Theme(val label: String) { Dark("Dark"), Light("Light"), System("System") }
}

/**
 * Settings dialog
 */
@Composable
fun SettingsDialog(
    settings: Settings,
    onSave: (Settings) -> Unit,
    onDismiss: () -> Unit,
    nativeEngine: NativeEngine? = null,
    onSaveProjectAs: (() -> Unit)? = null
) {
    var localSettings by remember { mutableStateOf(settings) }
    var showAdvanced by remember { mutableStateOf(false) }
    var showProjectMenu by remember { mutableStateOf(false) }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.5f))
            .pointerInput(Unit) {
                detectTapGestures(onTap = { onDismiss() })
            }
    ) {
        Card(
            modifier = Modifier
                .width(500.dp)
                .height(600.dp)
                .background(Color(0xFF1E1E1E)),
            elevation = 8.dp
        ) {
            Column(modifier = Modifier.fillMaxSize()) {
                // Header
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(56.dp)
                        .padding(horizontal = 16.dp)
                        .background(Color(0xFF252525)),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(text = "Settings", color = Color.White, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                    
                    // Project menu button
                    androidx.compose.material3.TextButton(
                        onClick = { showProjectMenu = !showProjectMenu },
                        modifier = Modifier.padding(end = 8.dp)
                    ) {
                        Row(
                            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.Center,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Icon(
                                painter = painterResource(id = android.R.drawable.ic_menu_save),
                                contentDescription = "Project",
                                tint = Color.Cyan,
                                modifier = Modifier.size(20.dp)
                            )
                            androidx.compose.foundation.layout.Box(modifier = Modifier.width(4.dp))
                            Text(text = "Project", color = Color.Cyan, fontSize = 14.sp)
                        }
                    }
                    
                    IconButton(onClick = onDismiss) {
                        Icon(painter = painterResource(id = android.R.drawable.ic_menu_close_clear_cancel), contentDescription = "Close")
                    }
                }

                Divider(color = Color.White.copy(alpha = 0.1f))

                // Project menu dropdown
                if (showProjectMenu) {
                    ProjectMenuDropdown(
                        nativeEngine = nativeEngine,
                        onDismiss = { showProjectMenu = false },
                        saveProjectAs = onSaveProjectAs
                    )
                    Divider(color = Color.White.copy(alpha = 0.1f))
                }

                // Content
                androidx.compose.foundation.lazy.LazyColumn(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(16.dp),
                    verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(16.dp)
                ) {
                    // General section
                    SettingsSection(title = "General") {
                        SettingsRow(
                            label = "Default Frame Rate",
                            description = "Timeline frame rate for new projects"
                        ) {
                            FilledTextField(
                                value = localSettings.defaultFrameRate.toString(),
                                onValueChange = { localSettings.defaultFrameRate = it.toDoubleOrNull() ?: 30.0 },
                                modifier = Modifier.width(80.dp),
                                singleLine = true,
                                textStyle = TextStyle(color = Color.White, fontSize = 14.sp)
                            )
                        }

                        SettingsRow(
                            label = "UI Scale",
                            description = "Global UI scaling factor"
                        ) {
                            Slider(
                                modifier = Modifier.width(200.dp),
                                value = (localSettings.uiScale - 0.5f) / 1.5f,
                                onValueChange = { localSettings.uiScale = it * 1.5f + 0.5f },
                                onValueChangeFinished = { },
                                colors = androidx.compose.material3.SliderDefaults.colors(thumbColor = Color.Cyan)
                            )
                            Text(text = String.format("%.1fx", localSettings.uiScale), color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
                        }

                        SettingsRow(
                            label = "Theme",
                            description = "Application color theme"
                        ) {
                            Row {
                                Settings.Theme.values().forEach { theme ->
                                    RadioButton(
                                        selected = localSettings.theme == theme,
                                        onClick = { localSettings.theme = theme },
                                        modifier = Modifier.padding(end = 16.dp),
                                        colors = androidx.compose.material3.RadioButtonDefaults.colors(selectedColor = Color.Cyan)
                                    )
                                    Text(text = theme.label, color = Color.White, fontSize = 14.sp)
                                }
                            }
                        }
                    }

                    Divider(color = Color.White.copy(alpha = 0.1f))

                    // Performance section
                    SettingsSection(title = "Performance") {
                        SettingsRow(
                            label = "Use Vulkan",
                            description = "Enable Vulkan renderer (requires restart)"
                        ) {
                            Switch(
                                checked = localSettings.useVulkan,
                                onCheckedChange = { localSettings.useVulkan = it },
                                colors = androidx.compose.material3.SwitchDefaults.colors(thumbColor = Color.Cyan)
                            )
                        }

                        SettingsRow(
                            label = "Max Concurrent Decoders",
                            description = "Maximum simultaneous video decoders"
                        ) {
                            Slider(
                                modifier = Modifier.width(200.dp),
                                value = (localSettings.maxConcurrentDecoders - 1) / 7f,
                                onValueChange = { localSettings.maxConcurrentDecoders = (it * 7f + 1).roundToInt() },
                                colors = androidx.compose.material3.SliderDefaults.colors(thumbColor = Color.Cyan)
                            )
                            Text(text = "${localSettings.maxConcurrentDecoders}", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
                        }

                        SettingsRow(
                            label = "Frame Cache Size (MB)",
                            description = "GPU memory budget for decoded frame cache"
                        ) {
                            Slider(
                                modifier = Modifier.width(200.dp),
                                value = (localSettings.frameCacheSizeMB - 64) / 512f,
                                onValueChange = { localSettings.frameCacheSizeMB = (it * 512f + 64).roundToInt() },
                                colors = androidx.compose.material3.SliderDefaults.colors(thumbColor = Color.Cyan)
                            )
                            Text(text = "${localSettings.frameCacheSizeMB} MB", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
                        }

                        SettingsRow(
                            label = "Enable HDR",
                            description = "High dynamic range output (requires device support)"
                        ) {
                            Switch(
                                checked = localSettings.enableHdr,
                                onCheckedChange = { localSettings.enableHdr = it },
                                colors = androidx.compose.material3.SwitchDefaults.colors(thumbColor = Color.Cyan)
                            )
                        }

                        SettingsRow(
                            label = "Thermal Throttling",
                            description = "Automatically reduce quality under thermal pressure"
                        ) {
                            Switch(
                                checked = localSettings.enableThermalThrottling,
                                onCheckedChange = { localSettings.enableThermalThrottling = it },
                                colors = androidx.compose.material3.SwitchDefaults.colors(thumbColor = Color.Cyan)
                            )
                        }
                    }

                    Divider(color = Color.White.copy(alpha = 0.1f))

                    // Advanced section (collapsible)
                    SettingsSection(title = "Advanced", isCollapsible = true, initiallyExpanded = showAdvanced, onExpandChange = { showAdvanced = it }) {
                        SettingsRow(
                            label = "Log Level",
                            description = "Minimum log level for native engine"
                        ) {
                            Settings.LogLevel.values().forEach { level ->
                                RadioButton(
                                    selected = localSettings.logLevel == level,
                                    onClick = { localSettings.logLevel = level },
                                    modifier = Modifier.padding(end = 16.dp),
                                    colors = androidx.compose.material3.RadioButtonDefaults.colors(selectedColor = Color.Cyan)
                                )
                                Text(text = level.label, color = Color.White, fontSize = 14.sp)
                            }
                        }

                        SettingsRow(
                            label = "Auto-save Interval",
                            description = "Seconds between automatic project saves"
                        ) {
                            Slider(
                                modifier = Modifier.width(200.dp),
                                value = (localSettings.autoSaveIntervalSec - 10) / 300f,
                                onValueChange = { localSettings.autoSaveIntervalSec = (it * 300f + 10).roundToInt() },
                                colors = androidx.compose.material3.SliderDefaults.colors(thumbColor = Color.Cyan)
                            )
                            Text(text = "${localSettings.autoSaveIntervalSec}s", color = Color.White.copy(alpha = 0.7f), fontSize = 12.sp)
                        }
                    }

                    Divider(color = Color.White.copy(alpha = 0.1f))

                    // Device info section
                    if (nativeEngine != null) {
                        SettingsSection(title = "Device Info") {
                            SettingsInfoRow("Renderer", "Vulkan (Adreno/Mali/PowerVR)")
                            SettingsInfoRow("API Level", "33+")
                            SettingsInfoRow("Max Texture Size", "8192")
                            SettingsInfoRow("Memory Heap", "256 MB")
                        }
                    }
                }

                // Footer buttons
                Divider(color = Color.White.copy(alpha = 0.1f))
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(56.dp)
                        .padding(horizontal = 16.dp),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.End
                ) {
                    Button(onClick = onDismiss, colors = androidx.compose.material3.ButtonDefaults.buttonColors(containerColor = Color(0xFF2D2D2D))) {
                        Text(text = "Cancel", color = Color.White)
                    }
                    androidx.compose.foundation.layout.Box(modifier = Modifier.width(8.dp))
                    Button(onClick = { onSave(localSettings); onDismiss() }, colors = androidx.compose.material3.ButtonDefaults.buttonColors(containerColor = Color.Cyan)) {
                        Text(text = "Save", color = Color.Black)
                    }
                }
            }
        }
    }
}

@Composable
fun SettingsSection(
    title: String,
    isCollapsible: Boolean = false,
    initiallyExpanded: Boolean = true,
    onExpandChange: ((Boolean) -> Unit)? = null,
    content: @Composable () -> Unit
) {
    var expanded by remember { mutableStateOf(initiallyExpanded) }
    
    Column {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(40.dp)
                .padding(horizontal = 16.dp),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(text = title, color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            if (isCollapsible) {
                IconButton(onClick = { expanded = !expanded; onExpandChange?.invoke(expanded) }) {
                    Icon(
                        painter = painterResource(id = if (expanded) android.R.drawable.ic_menu_remove else android.R.drawable.ic_menu_add),
                        contentDescription = if (expanded) "Collapse" else "Expand"
                    )
                }
            }
        }
        
        if (expanded || !isCollapsible) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, bottom = 8.dp),
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(12.dp)
            ) {
                content()
            }
        }
    }
}

@Composable
fun SettingsRow(
    label: String,
    description: String = "",
    content: @Composable () -> Unit
) {
    Column(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column {
                Text(text = label, color = Color.White, fontSize = 14.sp)
                if (description.isNotBlank()) {
                    Text(text = description, color = Color.White.copy(alpha = 0.5f), fontSize = 11.sp)
                }
            }
            content()
        }
    }
}

@Composable
fun SettingsInfoRow(label: String, value: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp, horizontal = 16.dp),
        horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween
    ) {
        Text(text = label, color = Color.White.copy(alpha = 0.7f), fontSize = 13.sp)
        Text(text = value, color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.Medium)
    }
}

/**
 * Project Menu Dropdown - New, Open, Save, Save As
 */
@Composable
fun ProjectMenuDropdown(
    nativeEngine: NativeEngine?,
    onDismiss: () -> Unit,
    saveProjectAs: (() -> Unit)? = null
) {
    val hasProject = nativeEngine?.hasProject() ?: false
    val projectName = nativeEngine?.getProjectName() ?: ""
    val isModified = nativeEngine?.isProjectModified() ?: false
    
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp)
            .background(Color(0xFF252525))
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(8.dp)
        ) {
            // Project info
            if (hasProject) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(
                        verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(2.dp)
                    ) {
                        Text(text = projectName, color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                        if (isModified) {
                            Text(text = "● Unsaved changes", color = Color.Yellow, fontSize = 11.sp)
                        }
                    }
                }
                Divider(color = Color.White.copy(alpha = 0.1f))
            }
            
            // Menu items
            ProjectMenuItem(
                label = "New Project",
                shortcut = "Ctrl+N",
                iconRes = android.R.drawable.ic_menu_add,
                enabled = true,
                onClick = {
                    // TODO: Show new project dialog
                    onDismiss()
                }
            )
            
            ProjectMenuItem(
                label = "Open Project...",
                shortcut = "Ctrl+O",
                iconRes = android.R.drawable.ic_menu_upload,
                enabled = true,
                onClick = {
                    // TODO: Show file picker
                    onDismiss()
                }
            )
            
            ProjectMenuItem(
                label = "Save Project",
                shortcut = "Ctrl+S",
                iconRes = android.R.drawable.ic_menu_save,
                enabled = hasProject && isModified,
                onClick = {
                    nativeEngine?.saveProject(null)
                    onDismiss()
                }
            )
            
            ProjectMenuItem(
                label = "Save Project As...",
                shortcut = "Ctrl+Shift+S",
                iconRes = android.R.drawable.ic_menu_save,
                enabled = hasProject,
                onClick = {
                    saveProjectAs?.invoke()
                    onDismiss()
                }
            )
        }
    }
}

@Composable
fun ProjectMenuItem(
    label: String,
    shortcut: String,
    iconRes: Int,
    enabled: Boolean = true,
    onClick: () -> Unit
) {
    androidx.compose.material3.TextButton(
        onClick = if (enabled) onClick else null,
        modifier = Modifier
            .fillMaxWidth()
            .height(40.dp)
            .padding(horizontal = 8.dp),
        colors = androidx.compose.material3.ButtonDefaults.textButtonColors(
            containerColor = if (enabled) Color.Transparent else Color.Transparent,
            contentColor = if (enabled) Color.White else Color.White.copy(alpha = 0.4f)
        )
    ) {
        Row(
            modifier = Modifier.fillMaxSize().padding(horizontal = 16.dp),
            horizontalArrangement = androidx.compose.foundation.layout.Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(
                horizontalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    painter = painterResource(id = iconRes),
                    contentDescription = label,
                    tint = if (enabled) Color.Cyan else Color.White.copy(alpha = 0.4f),
                    modifier = Modifier.size(20.dp)
                )
                Text(text = label, color = if (enabled) Color.White else Color.White.copy(alpha = 0.4f), fontSize = 14.sp)
            }
            Text(text = shortcut, color = Color.White.copy(alpha = 0.5f), fontSize = 11.sp, fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace)
        }
    }
}