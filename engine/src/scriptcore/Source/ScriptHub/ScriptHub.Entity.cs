namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;

public static partial class ScriptHub
{
    private static unsafe int GetEntityComponents(void** args, void** result)
    {
        var entityId = *(ulong*)args[0];

        if (World.Current is null)
            return 0;

        var handles = new List<long>();
        foreach (var type in World.Current.GetMonoComponentTypes(entityId))
        {
            if (NativeCalls.Native_ComponentExists(entityId, type))
                continue;
            if (!World.Current.TryGetComponent(entityId, type, out var comp))
                continue;

            var handle = NextHandle++;
            ObjectRegistry[handle] = comp;
            InstanceTypeCache[handle] = comp.GetType();
            handles.Add(handle);
        }

        var json = JsonSerializer.Serialize(handles);
        *result = (void*)Marshal.StringToCoTaskMemUTF8(json);
        return handles.Count;
    }

    private static unsafe int AddEntityComponent(void** args)
    {
        var entityId = *(ulong*)args[0];
        var fullName = Marshal.PtrToStringUTF8((IntPtr)args[1]);

        if (World.Current is null || string.IsNullOrWhiteSpace(fullName))
            return 0;

        Type componentType = null;
        foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
        {
            componentType = asm.GetType(fullName, throwOnError: false, ignoreCase: false);
            if (componentType is not null) break;
        }

        if (componentType is null)
        {
            var shortName = fullName.Contains('.') ? fullName.Split('.').Last() : fullName;
            foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
            {
                componentType = asm.GetTypes()
                    .FirstOrDefault(t => t.Name == shortName && typeof(Component).IsAssignableFrom(t));
                if (componentType is not null) break;
            }
        }

        if (componentType is null || !typeof(Component).IsAssignableFrom(componentType))
            return 0;

        if (Activator.CreateInstance(componentType) is not Component component)
            return 0;

        var entity = new Entity(entityId);
        var generic = typeof(Entity).GetMethods(BindingFlags.Instance | BindingFlags.Public)
            .FirstOrDefault(m => m.Name == "AddComponent" && m.IsGenericMethodDefinition && m.GetParameters().Length == 1);
        if (generic is null) return 0;

        var closed = generic.MakeGenericMethod(componentType);
        closed.Invoke(entity, new object[] { component });
        return 1;
    }

    private static unsafe int RemoveEntity(void** args)
    {
        var entityId = *(ulong*)args[0];
        World.Current?.RemoveEntityLocal(entityId);
        return 1;
    }
}
