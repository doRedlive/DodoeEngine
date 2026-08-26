namespace GreenCake;

using System;
using System.Linq;
using System.Threading.Tasks;

internal static class CakeSystemScheduler
{
    private static CakeSystem[] _cachedSystems = Array.Empty<CakeSystem>();
    private static readonly CakeTaskGraph _graph = new();
    private static bool _dirty = true;

    public static void InvalidateCache()
    {
        _dirty = true;
    }

    private static void Refresh()
    {
        if (!_dirty) return;
        _cachedSystems = ScriptHub.ObjectRegistry.Values.OfType<CakeSystem>().ToArray();
        _graph.Build(_cachedSystems);
        _dirty = false;
    }

    public static void ExecuteOnCreate()
    {
        Refresh();
        foreach (var system in _cachedSystems)
        {
            try { system.OnCreate(); }
            catch (Exception e) { Debug.LogError($"OnCreate error in {system.GetType().Name}: {e}"); }
        }
    }

    public static void ExecuteOnUpdate()
    {
        Refresh();
        var levels = _graph.Levels;
        foreach (var level in levels)
        {
            if (level.Length == 1)
            {
                var sys = level[0];
                try { sys.OnUpdate(); }
                catch (Exception e) { Debug.LogError($"OnUpdate error in {sys.GetType().Name}: {e}"); }
            }
            else
            {
                Parallel.ForEach(level, sys =>
                {
                    try { sys.OnUpdate(); }
                    catch (Exception e) { Debug.LogError($"OnUpdate error in {sys.GetType().Name}: {e}"); }
                });
            }
        }
    }

    public static void ExecuteOnFixedUpdate()
    {
        Refresh();
        foreach (var system in _cachedSystems)
        {
            try { system.OnFixedUpdate(); }
            catch (Exception e) { Debug.LogError($"OnFixedUpdate error in {system.GetType().Name}: {e}"); }
        }
    }

    public static void ExecuteOnDestroy()
    {
        Refresh();
        foreach (var system in _cachedSystems)
        {
            try { system.OnDestroy(); }
            catch (Exception e) { Debug.LogError($"OnDestroy error in {system.GetType().Name}: {e}"); }
        }
    }
}
