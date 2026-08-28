namespace GreenCake;

using System.IO;

public static class FilePath
{
    public static string Resolve(string path)
    {
        if (string.IsNullOrEmpty(path) || Path.IsPathRooted(path))
            return path;

        string assetDir = NativeCalls.Native_GetAssetDirectory();

        if (!string.IsNullOrEmpty(assetDir))
        {
            string combined = Path.GetFullPath(Path.Combine(assetDir, path));
            if (File.Exists(combined))
                return combined;

            Debug.Log($"[FilePath] file not found at '{combined}', fallback to CWD");
        }

        return Path.GetFullPath(path);
    }
}
