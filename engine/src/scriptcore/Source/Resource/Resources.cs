namespace GreenCake;

using System;
using System.IO;

public static class Resources
{
    public static T? Load<T>(string path) where T : Object
    {
        Object.EnsureRegistered<T>();
        var typeName = Object.GetNativeTypeName(typeof(T));
        if (typeName is null) {
            throw new NotSupportedException(
                $"Resources.Load<{typeof(T).Name}> has no registered native type. " +
                "Register it via Object.RegisterType<T> in the wrapper's static constructor.");
        }

        var fullPath = FilePath.Resolve(path);
        if (!File.Exists(fullPath)) return null;

        int id = NativeCalls.Native_LoadObject(fullPath, typeName);
        return Object.FindObjectFromInstanceID<T>(id);
    }
}
