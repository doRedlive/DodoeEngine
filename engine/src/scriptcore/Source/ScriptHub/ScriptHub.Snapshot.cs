namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;

public static partial class ScriptHub
{
    private static unsafe int SnapshotAll(void** args, void** result)
    {
        var snapshot = new Dictionary<long, Dictionary<string, object>>();
        foreach (var (handle, obj) in ObjectRegistry)
        {
            var fields = obj.GetType().GetFields(
                BindingFlags.Public | BindingFlags.Instance);
            var values = new Dictionary<string, object>();
            foreach (var f in fields)
            {
                try { values[f.Name] = f.GetValue(obj); }
                catch { }
            }
            if (values.Count > 0)
                snapshot[handle] = values;
        }

        var json = JsonSerializer.Serialize(snapshot);
        *result = (void*)Marshal.StringToCoTaskMemUTF8(json);
        return 1;
    }

    private static unsafe int RestoreAll(void** args)
    {
        var json = Marshal.PtrToStringUTF8((IntPtr)args[0]);
        if (string.IsNullOrEmpty(json)) return 0;

        var snapshot = JsonSerializer
            .Deserialize<Dictionary<long, Dictionary<string, JsonElement>>>(json);
        if (snapshot == null) return 0;

        foreach (var (handle, values) in snapshot)
        {
            if (!ObjectRegistry.TryGetValue(handle, out var obj)) continue;
            foreach (var (fieldName, jsonValue) in values)
            {
                var field = obj.GetType().GetField(fieldName,
                    BindingFlags.Public | BindingFlags.Instance);
                if (field == null) continue;

                try
                {
                    var converted = JsonSerializer.Deserialize(
                        jsonValue.GetRawText(), field.FieldType);
                    if (converted != null && converted.GetType() == field.FieldType)
                        field.SetValue(obj, converted);
                }
                catch { }
            }
        }
        return 1;
    }
}
