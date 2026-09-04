namespace GreenCake.ModelImporter;

using System;
using System.IO;

public static class ModelImporter
{
    [ToolMenuItem("Model/Import backpack.obj into Scene")]
    public static void ImportBackpackIntoScene()
    {
        const string relativePath = "Models/backpack/backpack.obj";
        string fullPath = FilePath.Resolve(relativePath);
        if (!File.Exists(fullPath))
        {
            Debug.LogError($"[ModelImporter] Model file not found: {fullPath}");
            return;
        }

        NativeCalls.Native_SceneImportModel(fullPath);
        Debug.Log($"[ModelImporter] Imported '{fullPath}' into the active scene.");
    }
}
