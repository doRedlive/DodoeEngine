namespace GreenCake;

using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;

public static partial class ScriptHub
{
    private static unsafe int GetField(void** args, void** result)
    {
        var handle = *(long*)args[0];
        var fieldName = Marshal.PtrToStringUTF8((IntPtr)args[1]);

        if (!ObjectRegistry.TryGetValue(handle, out var obj))
            return 0;

        var field = obj.GetType().GetField(fieldName,
            BindingFlags.Public | BindingFlags.Instance);
        if (field == null) return 0;

        var value = field.GetValue(obj);
        if (value == null) return 0;

        var fc = field.FieldType;
        if (fc == typeof(float))
            *(float*)result = (float)value;
        else if (fc == typeof(double))
            *(double*)result = (double)value;
        else if (fc == typeof(int))
            *(int*)result = (int)value;
        else if (fc == typeof(uint))
            *(uint*)result = (uint)value;
        else if (fc == typeof(long))
            *(long*)result = (long)value;
        else if (fc == typeof(ulong))
            *(ulong*)result = (ulong)value;
        else if (fc == typeof(short))
            *(short*)result = (short)value;
        else if (fc == typeof(ushort))
            *(ushort*)result = (ushort)value;
        else if (fc == typeof(byte))
            *(byte*)result = (byte)value;
        else if (fc == typeof(sbyte))
            *(sbyte*)result = (sbyte)value;
        else if (fc == typeof(bool))
            *(bool*)result = (bool)value;
        else if (fc == typeof(char))
            *(char*)result = (char)value;
        else
        {
            var json = JsonSerializer.Serialize(value);
            *result = (void*)Marshal.StringToCoTaskMemUTF8(json);
        }
        return 1;
    }

    private static unsafe int SetField(void** args)
    {
        var handle = *(long*)args[0];
        var fieldName = Marshal.PtrToStringUTF8((IntPtr)args[1]);

        if (!ObjectRegistry.TryGetValue(handle, out var obj))
            return 0;

        var field = obj.GetType().GetField(fieldName,
            BindingFlags.Public | BindingFlags.Instance);
        if (field == null) return 0;

        var fc = field.FieldType;
        var valuePtr = args[2];

        if (fc == typeof(float))
            field.SetValue(obj, *(float*)valuePtr);
        else if (fc == typeof(double))
            field.SetValue(obj, *(double*)valuePtr);
        else if (fc == typeof(int))
            field.SetValue(obj, *(int*)valuePtr);
        else if (fc == typeof(uint))
            field.SetValue(obj, *(uint*)valuePtr);
        else if (fc == typeof(long))
            field.SetValue(obj, *(long*)valuePtr);
        else if (fc == typeof(ulong))
            field.SetValue(obj, *(ulong*)valuePtr);
        else if (fc == typeof(short))
            field.SetValue(obj, *(short*)valuePtr);
        else if (fc == typeof(ushort))
            field.SetValue(obj, *(ushort*)valuePtr);
        else if (fc == typeof(byte))
            field.SetValue(obj, *(byte*)valuePtr);
        else if (fc == typeof(sbyte))
            field.SetValue(obj, *(sbyte*)valuePtr);
        else if (fc == typeof(bool))
            field.SetValue(obj, *(bool*)valuePtr);
        else if (fc == typeof(char))
            field.SetValue(obj, *(char*)valuePtr);
        else if (fc == typeof(Vector2f))
            field.SetValue(obj, *(Vector2f*)valuePtr);
        else if (fc == typeof(Vector3f))
            field.SetValue(obj, *(Vector3f*)valuePtr);
        else
            return 0;

        return 1;
    }
}
