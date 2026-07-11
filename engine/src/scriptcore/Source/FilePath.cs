namespace GreenCake;

using System.IO;

public static class FilePath
{
    public static string Resolve(string path)
    {
        if (string.IsNullOrEmpty(path) || Path.IsPathRooted(path))
            return path;
        return Path.Combine(InternalCalls.Native_GetAssetDirectory(), path);
    }
}
