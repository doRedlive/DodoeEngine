namespace OnlyOne;

using GreenCake;

public static class ModelImporter
{
    private const string ModelPath = "Models/backpack/backpack.obj";

    [ToolMenuItem("Tools/Import 3D Model")]
    public static void ImportModel()
    {
        Scene scene = SceneManager.ActiveScene;
        if (scene == null)
        {
            Debug.LogError("No active scene. Open a scene before importing.");
            return;
        }

        Mesh mesh = Mesh.Load(ModelPath);
        if (mesh == null)
        {
            Debug.LogError($"Failed to load mesh '{ModelPath}'.");
            return;
        }

        GameObject go = scene.CreateGameObject("ImportedModel");
        go.AddComponent<MeshRendererComponent>().Mesh = mesh;
        go.Transform.Position = new Vector3f(0f, 0f, 0f);
        Debug.Log($"Imported model '{ModelPath}'.");
    }

    [ToolMenuItem("Tools/Save Scene")]
    public static void SaveScene()
    {
        Scene scene = SceneManager.ActiveScene;
        if (scene == null)
        {
            Debug.LogError("No active scene to save.");
            return;
        }

        if (scene.Save())
            Debug.Log($"Scene '{scene.Name}' saved.");
        else
            Debug.LogError($"Failed to save scene '{scene.Name}'.");
    }
}
