namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;
using GreenCake.Editor;

public static partial class ScriptHub
{
    private sealed class EditorWindowEntry
    {
        public string Id = string.Empty;
        public string Title = string.Empty;
        public Type? Type;
    }

    private static readonly object EditorRegistryLock = new();
    private static bool s_editorRegistryLoaded;
    private static readonly Dictionary<string, Type> s_inspectorEditors = new();
    private static readonly List<EditorWindowEntry> s_editorWindows = new();
    private static readonly Dictionary<Type, object> s_editorInstances = new();

    private static void ClearEditorRegistry()
    {
        lock (EditorRegistryLock)
        {
            s_editorRegistryLoaded = false;
            s_inspectorEditors.Clear();
            s_editorWindows.Clear();
            s_editorInstances.Clear();
        }
    }

    private static void EnsureEditorRegistry()
    {
        lock (EditorRegistryLock)
        {
            if (s_editorRegistryLoaded)
                return;
            s_editorRegistryLoaded = true;

            var assemblies = new List<Assembly>();
            if (AppAlc != null)
            {
                foreach (var asm in AppAlc.Assemblies)
                {
                    if (!assemblies.Contains(asm))
                        assemblies.Add(asm);
                }
            }
            var core = typeof(ScriptHub).Assembly;
            if (!assemblies.Contains(core))
                assemblies.Add(core);

            foreach (var asm in assemblies)
            {
                Type[] types;
                try
                {
                    types = asm.GetTypes();
                }
                catch (ReflectionTypeLoadException ex)
                {
                    types = ex.Types ?? Array.Empty<Type>();
                }
                catch
                {
                    continue;
                }

                foreach (var type in types)
                {
                    if (type == null || type.IsAbstract)
                        continue;

                    try
                    {
                        var customEditor = type.GetCustomAttribute<CustomEditorAttribute>();
                        if (customEditor != null && customEditor.TargetType != null &&
                            typeof(Editor).IsAssignableFrom(type))
                        {
                            string full = customEditor.TargetType.FullName ?? string.Empty;
                            string simple = customEditor.TargetType.Name;
                            if (full.Length > 0 && !s_inspectorEditors.ContainsKey(full))
                                s_inspectorEditors[full] = type;
                            if (simple.Length > 0 && !s_inspectorEditors.ContainsKey(simple))
                                s_inspectorEditors[simple] = type;
                            continue;
                        }

                        var windowAttribute = type.GetCustomAttribute<EditorWindowAttribute>();
                        if (windowAttribute != null && typeof(EditorWindow).IsAssignableFrom(type))
                        {
                            s_editorWindows.Add(new EditorWindowEntry
                            {
                                Id = type.FullName ?? type.Name,
                                Title = string.IsNullOrEmpty(windowAttribute.Title) ? type.Name : windowAttribute.Title,
                                Type = type,
                            });
                        }
                    }
                    catch
                    {
                    }
                }
            }
        }
    }

    private static object GetEditorInstance(Type type)
    {
        if (s_editorInstances.TryGetValue(type, out var existing))
            return existing;
        var instance = Activator.CreateInstance(type)!;
        s_editorInstances[type] = instance;
        return instance;
    }

    private static string? GetInspectorJson(string? typeName)
    {
        if (string.IsNullOrEmpty(typeName))
            return null;
        EnsureEditorRegistry();
        if (!s_inspectorEditors.TryGetValue(typeName, out var type))
            return null;
        try
        {
            var editor = (Editor)GetEditorInstance(type);
            var gui = new EditorGUI();
            editor.OnInspectorGUI(gui);
            return gui.ToJson();
        }
        catch (Exception ex)
        {
            Debug.LogError($"Custom editor for '{typeName}' failed: {ex.Message}");
            return null;
        }
    }

    private static string ListWindowsJson()
    {
        EnsureEditorRegistry();
        var windows = new List<Dictionary<string, object?>>();
        foreach (var window in s_editorWindows)
        {
            windows.Add(new Dictionary<string, object?>
            {
                ["id"] = window.Id,
                ["title"] = window.Title,
            });
        }
        return JsonSerializer.Serialize(new Dictionary<string, object?> { ["windows"] = windows });
    }

    private static string? GetWindowJson(string? id)
    {
        if (string.IsNullOrEmpty(id))
            return null;
        EnsureEditorRegistry();
        foreach (var window in s_editorWindows)
        {
            if (window.Id != id || window.Type == null)
                continue;
            try
            {
                var instance = (EditorWindow)GetEditorInstance(window.Type);
                var gui = new EditorGUI();
                instance.OnGUI(gui);
                return gui.ToJson();
            }
            catch (Exception ex)
            {
                Debug.LogError($"Editor window '{id}' failed: {ex.Message}");
                return null;
            }
        }
        return null;
    }

    private static void DispatchEditorEvent(string? owner, string? ownerId, string? controlId,
                                            string? eventName, string? valueJson)
    {
        EnsureEditorRegistry();
        var editorEvent = new EditorEvent(owner ?? string.Empty, ownerId ?? string.Empty,
            controlId ?? string.Empty, eventName ?? string.Empty, valueJson ?? string.Empty);
        try
        {
            if (owner == "inspector")
            {
                if (ownerId != null && s_inspectorEditors.TryGetValue(ownerId, out var type))
                    ((Editor)GetEditorInstance(type)).OnEvent(editorEvent);
            }
            else if (owner == "window")
            {
                foreach (var window in s_editorWindows)
                {
                    if (window.Id == ownerId && window.Type != null)
                    {
                        ((EditorWindow)GetEditorInstance(window.Type)).OnEvent(editorEvent);
                        break;
                    }
                }
            }
        }
        catch (Exception ex)
        {
            Debug.LogError($"Editor event '{eventName}' failed: {ex.Message}");
        }
    }

    private static unsafe int EditorGetInspectorUI(void** args, void** result)
    {
        if (result == null)
            return 0;
        var typeName = args != null && args[0] != null
            ? Marshal.PtrToStringUTF8((IntPtr)args[0])
            : null;
        var json = GetInspectorJson(typeName);
        if (json == null)
            return 0;
        *result = (void*)Marshal.StringToCoTaskMemUTF8(json);
        return 1;
    }

    private static unsafe int EditorListWindows(void** args, void** result)
    {
        if (result == null)
            return 0;
        var json = ListWindowsJson();
        *result = (void*)Marshal.StringToCoTaskMemUTF8(json);
        return 1;
    }

    private static unsafe int EditorGetWindowUI(void** args, void** result)
    {
        if (result == null)
            return 0;
        var id = args != null && args[0] != null
            ? Marshal.PtrToStringUTF8((IntPtr)args[0])
            : null;
        var json = GetWindowJson(id);
        if (json == null)
            return 0;
        *result = (void*)Marshal.StringToCoTaskMemUTF8(json);
        return 1;
    }

    private static unsafe int EditorDispatchEvent(void** args, void** result)
    {
        string? owner = args != null && args[0] != null ? Marshal.PtrToStringUTF8((IntPtr)args[0]) : null;
        string? ownerId = args != null && args[1] != null ? Marshal.PtrToStringUTF8((IntPtr)args[1]) : null;
        string? controlId = args != null && args[2] != null ? Marshal.PtrToStringUTF8((IntPtr)args[2]) : null;
        string? eventName = args != null && args[3] != null ? Marshal.PtrToStringUTF8((IntPtr)args[3]) : null;
        string? valueJson = args != null && args[4] != null ? Marshal.PtrToStringUTF8((IntPtr)args[4]) : null;
        DispatchEditorEvent(owner, ownerId, controlId, eventName, valueJson);
        return 1;
    }
}
