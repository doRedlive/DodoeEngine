namespace GreenCake;

using System;
using System.Linq;
using System.Reflection;

public static partial class ScriptHub
{
    private static unsafe int InvokeLifecycle(void** args, string methodName)
    {
        var systemTypes = ObjectRegistry.Values
            .Where(o => o.GetType().IsSubclassOf(typeof(CakeSystem)))
            .ToList();

        foreach (var system in systemTypes)
        {
            var m = system.GetType().GetMethod(methodName,
                BindingFlags.Public | BindingFlags.Instance | BindingFlags.NonPublic);
            if (m != null && m.GetParameters().Length == 0)
            {
                try { m.Invoke(system, null); }
                catch (Exception e) { Debug.LogError($"{methodName} error in {system.GetType().Name}: {e}"); }
            }
        }
        return 1;
    }
}
