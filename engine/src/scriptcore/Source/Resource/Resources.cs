namespace GreenCake;

using System;
using System.IO;

public static class Resources
{
    public static T? Load<T>(string path) where T : Object
    {
        if (typeof(T) == typeof(Texture)) {
            var fullPath = FilePath.Resolve(path);
            if (!File.Exists(fullPath)) return null;
            int id = NativeCalls.Native_TextureLoad(fullPath);
            return Object.FindObjectFromInstanceID<T>(id);
        }

        if (typeof(T) == typeof(Sprite)) {
            var fullPath = FilePath.Resolve(path);
            if (!File.Exists(fullPath)) return null;
            int id = NativeCalls.Native_SpriteLoad(fullPath);
            return Object.FindObjectFromInstanceID<T>(id);
        }

        throw new NotSupportedException($"Resources.Load<{typeof(T).Name}> is not supported yet.");
    }
}
