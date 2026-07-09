namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

public static class ExternalCalls
{
    public static Type[] GetEntityMonoComponentTypes(ulong entityId)
    {
        if (World.Current is null)
            return Array.Empty<Type>();

        var result = new List<Type>();
        foreach (var type in World.Current.GetMonoComponentTypes(entityId))
        {
            if (InternalCalls.Native_ComponentExists(entityId, type))
                continue;
            result.Add(type);
        }

        return result.ToArray();
    }

    public static Component[] GetEntityMonoComponents(ulong entityId)
    {
        if (World.Current is null)
            return Array.Empty<Component>();

        var result = new List<Component>();
        foreach (var type in World.Current.GetMonoComponentTypes(entityId))
        {
            if (InternalCalls.Native_ComponentExists(entityId, type))
                continue;

            if (World.Current.TryGetComponent(entityId, type, out Component component))
                result.Add(component);
        }

        return result.ToArray();
    }

    public static bool AddEntityMonoComponent(ulong entityId, string fullName)
    {
        if (World.Current is null)
            return false;

        if (string.IsNullOrWhiteSpace(fullName))
            return false;

        Type componentType = null;
        foreach (Assembly assembly in AppDomain.CurrentDomain.GetAssemblies())
        {
            componentType = assembly.GetType(fullName, throwOnError: false, ignoreCase: false);
            if (componentType is not null)
                break;
        }

        if (componentType is null)
        {
            string shortName = fullName.Contains('.') ? fullName.Split('.').Last() : fullName;
            foreach (Assembly assembly in AppDomain.CurrentDomain.GetAssemblies())
            {
                componentType = assembly
                    .GetTypes()
                    .FirstOrDefault(t => t.Name == shortName && typeof(Component).IsAssignableFrom(t));
                if (componentType is not null)
                    break;
            }
        }

        if (componentType is null || !typeof(Component).IsAssignableFrom(componentType))
            return false;

        Component component = Activator.CreateInstance(componentType) as Component;
        if (component is null)
            return false;

        var entity = new Entity(entityId);
        MethodInfo generic = typeof(Entity)
            .GetMethods(BindingFlags.Instance | BindingFlags.Public)
            .FirstOrDefault(m => m.Name == "AddComponent" && m.IsGenericMethodDefinition && m.GetParameters().Length == 1);
        if (generic is null)
            return false;

        MethodInfo closed = generic.MakeGenericMethod(componentType);
        closed.Invoke(entity, new object[] { component });
        return true;
    }

    public static void RemoveEntityFromManagedWorld(ulong entityId)
    {
        World.Current?.RemoveEntityLocal(entityId);
    }
}
