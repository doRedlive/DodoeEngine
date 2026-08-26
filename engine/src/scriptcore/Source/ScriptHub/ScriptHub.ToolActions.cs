namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;

public static partial class ScriptHub
{
    private sealed class ToolActionEntry
    {
        public string Path;
        public MethodInfo Method;
    }

    private static readonly object ToolActionsLock = new();
    private static List<ToolActionEntry> s_toolActions;

    private static void ClearToolActionCache()
    {
        lock (ToolActionsLock)
        {
            s_toolActions = null;
        }
    }

    private static List<ToolActionEntry> EnsureToolActionsLoaded()
    {
        lock (ToolActionsLock)
        {
            if (s_toolActions != null)
                return s_toolActions;

            var actions = new List<ToolActionEntry>();
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
                    if (type == null)
                        continue;

                    MethodInfo[] methods;
                    try
                    {
                        methods = type.GetMethods(
                            BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
                    }
                    catch
                    {
                        continue;
                    }

                    foreach (var method in methods)
                    {
                        var attribute = method.GetCustomAttribute<ToolMenuItemAttribute>();
                        if (attribute == null || string.IsNullOrWhiteSpace(attribute.Path))
                            continue;

                        var parameters = method.GetParameters();
                        if (parameters.Length > 1)
                            continue;
                        if (parameters.Length == 1 && parameters[0].ParameterType != typeof(string))
                            continue;

                        actions.Add(new ToolActionEntry { Path = attribute.Path, Method = method });
                    }
                }
            }

            actions.Sort((a, b) => string.CompareOrdinal(a.Path, b.Path));
            s_toolActions = actions;
            return s_toolActions;
        }
    }

    private static unsafe int ListToolActions(void** args, void** result)
    {
        if (result == null)
            return 0;

        var actions = EnsureToolActionsLoaded();
        using var stream = new System.IO.MemoryStream();
        using (var writer = new Utf8JsonWriter(stream))
        {
            writer.WriteStartArray();
            foreach (var entry in actions)
                writer.WriteStringValue(entry.Path);
            writer.WriteEndArray();
        }
        var json = Encoding.UTF8.GetString(stream.ToArray());
        *result = (void*)Marshal.StringToCoTaskMemUTF8(json);
        return 1;
    }

    private static unsafe int InvokeToolAction(void** args, void** result)
    {
        if (result == null)
            return 0;

        var name = args != null && args[0] != null
            ? Marshal.PtrToStringUTF8((IntPtr)args[0])
            : null;

        string json;
        try
        {
            ToolActionEntry target = null;
            var actions = EnsureToolActionsLoaded();
            foreach (var entry in actions)
            {
                if (entry.Path == name)
                {
                    target = entry;
                    break;
                }
            }

            if (target == null)
            {
                json = JsonSerializer.Serialize(new
                {
                    ok = false,
                    error = $"Tool action not found: {name}"
                });
                *result = (void*)Marshal.StringToCoTaskMemUTF8(json);
                return 1;
            }

            var method = target.Method;
            if (method.GetParameters().Length == 1)
                method.Invoke(null, new object[] { name });
            else
                method.Invoke(null, null);

            json = JsonSerializer.Serialize(new { ok = true, error = string.Empty });
        }
        catch (Exception ex)
        {
            json = JsonSerializer.Serialize(new
            {
                ok = false,
                error = ex.ToString()
            });
        }

        *result = (void*)Marshal.StringToCoTaskMemUTF8(json);
        return 1;
    }
}
