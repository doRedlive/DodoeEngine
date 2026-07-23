namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Threading.Tasks;

public enum LoadSceneMode
{
    Single = 0,
    Additive = 1,
}

public static class SceneManager
{
    private static readonly Dictionary<string, Scene> _scenes = new();
    private static Scene _activeScene;

    public static Scene ActiveScene => _activeScene;
    public static int SceneCount => _scenes.Count;

    public static event Action<Scene> OnSceneLoaded;
    public static event Action<Scene> OnSceneUnloaded;
    public static event Action<Scene, Scene> OnActiveSceneChanged;

    public static Scene LoadScene(string sceneName, LoadSceneMode mode = LoadSceneMode.Single)
    {
        if (string.IsNullOrEmpty(sceneName))
            throw new ArgumentNullException(nameof(sceneName));

        if (_scenes.TryGetValue(sceneName, out var existing))
        {
            SetActiveScene(existing);
            return existing;
        }

        if (mode == LoadSceneMode.Single)
        {
            foreach (var s in GetAllScenes())
            {
                if (s._isLoaded)
                    UnloadScene(s._name);
            }
        }

        var scene = new Scene(sceneName);
        _scenes[sceneName] = scene;
        scene._isLoaded = true;

        NativeCalls.Native_WorldLoadScene(sceneName, (int)mode);

        scene.NotifyLoad();
        OnSceneLoaded?.Invoke(scene);

        if (_activeScene == null)
            SetActiveScene(scene);

        return scene;
    }

    public static async Task<Scene> LoadSceneAsync(string sceneName, LoadSceneMode mode = LoadSceneMode.Single)
    {
        if (string.IsNullOrEmpty(sceneName))
            throw new ArgumentNullException(nameof(sceneName));

        if (_scenes.TryGetValue(sceneName, out var existing))
        {
            SetActiveScene(existing);
            return existing;
        }

        if (mode == LoadSceneMode.Single)
        {
            foreach (var s in GetAllScenes())
            {
                if (s._isLoaded)
                    UnloadScene(s._name);
            }
        }

        var scene = new Scene(sceneName);
        _scenes[sceneName] = scene;

        int token = NativeCalls.Native_WorldLoadSceneAsync(sceneName, (int)mode);

        while (!NativeCalls.Native_WorldIsLoadComplete(token))
        {
            await Task.Yield();
        }

        scene._isLoaded = true;
        scene.NotifyLoad();
        OnSceneLoaded?.Invoke(scene);

        if (_activeScene == null)
            SetActiveScene(scene);

        return scene;
    }

    public static void UnloadScene(string sceneName)
    {
        if (!_scenes.TryGetValue(sceneName, out var scene))
            return;

        if (_activeScene == scene)
            SetActiveScene(null);

        scene.NotifyUnload();

        NativeCalls.Native_WorldUnloadScene(sceneName);

        _scenes.Remove(sceneName);
        OnSceneUnloaded?.Invoke(scene);
    }

    public static Scene GetScene(string name)
    {
        _scenes.TryGetValue(name, out var scene);
        return scene;
    }

    public static Scene[] GetAllScenes()
    {
        var result = new Scene[_scenes.Count];
        _scenes.Values.CopyTo(result, 0);
        return result;
    }

    public static void SetActiveScene(Scene scene)
    {
        if (scene == _activeScene)
            return;

        var previous = _activeScene;
        _activeScene = scene;

        if (previous != null)
            previous.NotifyStop();

        if (scene != null)
            scene.NotifyStart();

        OnActiveSceneChanged?.Invoke(previous, scene);
    }

    internal static void Reset()
    {
        foreach (var scene in _scenes.Values)
            scene.NotifyUnload();
        _scenes.Clear();
        _activeScene = null;
        OnSceneLoaded = null;
        OnSceneUnloaded = null;
        OnActiveSceneChanged = null;
    }
}
