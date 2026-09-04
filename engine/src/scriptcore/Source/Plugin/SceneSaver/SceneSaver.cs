namespace GreenCake.SceneSaver;

using System;

public static class SceneSaver
{
    [ToolMenuItem("Scene/Save Active Scene")]
    public static void SaveActiveScene()
    {
        Scene scene = SceneManager.ActiveScene;
        if (scene == null)
        {
            Debug.LogError("[SceneSaver] No active scene to save.");
            return;
        }

        if (scene.Save())
            Debug.Log($"[SceneSaver] Scene '{scene.Name}' saved.");
        else
            Debug.LogError($"[SceneSaver] Failed to save scene '{scene.Name}'.");
    }
}
