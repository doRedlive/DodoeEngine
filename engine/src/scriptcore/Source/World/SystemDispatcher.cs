namespace GreenCake;

using System;
using System.Linq;

internal static class SystemDispatcher
{
    private static CakeSystem[] _cachedSystems = Array.Empty<CakeSystem>();
    private static bool _dirty = true;

    public static void InvalidateCache()
    {
        _dirty = true;
    }

    private static void Refresh()
    {
        if (!_dirty) return;
        _cachedSystems = ScriptHub.ObjectRegistry.Values
            .OfType<CakeSystem>()
            .ToArray();
        _dirty = false;
    }

    public static void OnCreate()
    {
        Refresh();
        foreach (var system in _cachedSystems)
        {
            try { system.OnCreate(); }
            catch (Exception e) { Debug.LogError($"OnCreate error in {system.GetType().Name}: {e}"); }
        }
    }

    public static void OnUpdate()
    {
        Refresh();
        foreach (var system in _cachedSystems)
        {
            try { system.OnUpdate(); }
            catch (Exception e) { Debug.LogError($"OnUpdate error in {system.GetType().Name}: {e}"); }
        }
    }

    public static void OnDestroy()
    {
        Refresh();
        foreach (var system in _cachedSystems)
        {
            try { system.OnDestroy(); }
            catch (Exception e) { Debug.LogError($"OnDestroy error in {system.GetType().Name}: {e}"); }
        }
    }
}
