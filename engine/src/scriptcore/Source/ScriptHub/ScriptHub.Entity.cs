namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;

public static partial class ScriptHub
{
    private static readonly Dictionary<ulong, List<long>> EntityComponentHandles = new();

    private static void RemoveEntityComponentHandles(ulong entityId)
    {
        if (!EntityComponentHandles.TryGetValue(entityId, out var trackedHandles)) return;
        foreach (var handle in trackedHandles)
        {
            ObjectRegistry.Remove(handle);
            InstanceTypeCache.Remove(handle);
        }
        EntityComponentHandles.Remove(entityId);
    }

    private static Type FindComponentType(string fullName)
    {
        if (string.IsNullOrWhiteSpace(fullName)) return null;

        var asms = new List<System.Reflection.Assembly>(AppDomain.CurrentDomain.GetAssemblies());
        if (AppAlc != null)
            asms.AddRange(AppAlc.Assemblies);

        foreach (var asm in asms)
        {
            var type = asm.GetType(fullName, throwOnError: false, ignoreCase: false);
            if (type is not null && typeof(CakeComponent).IsAssignableFrom(type))
                return type;
        }

        var shortName = fullName.Contains('.') ? fullName.Split('.').Last() : fullName;
        foreach (var asm in asms)
        {
            var type = EnumerateTypes(asm)
                .FirstOrDefault(t => t.Name == shortName && typeof(CakeComponent).IsAssignableFrom(t));
            if (type is not null) return type;
        }
        return null;
    }

    private static unsafe int GetEntityComponents(void** args, void** result)
    {
        var entityId = *(ulong*)args[0];

        if (World.Current is null)
            return 0;

        if (!EntityComponentHandles.TryGetValue(entityId, out var trackedHandles))
        {
            trackedHandles = new List<long>();
            EntityComponentHandles[entityId] = trackedHandles;
        }

        foreach (var oldHandle in trackedHandles)
        {
            ObjectRegistry.Remove(oldHandle);
            InstanceTypeCache.Remove(oldHandle);
        }
        trackedHandles.Clear();

        var handles = new List<long>();
        foreach (var type in World.Current.GetManagedComponentTypes(entityId))
        {
            if (NativeCalls.Native_ComponentExists(entityId, type))
                continue;
            if (!World.Current.TryGetComponent(entityId, type, out var comp))
                continue;

            var handle = NextHandle++;
            ObjectRegistry[handle] = comp;
            InstanceTypeCache[handle] = comp.GetType();
            trackedHandles.Add(handle);
            handles.Add(handle);
        }

        var json = JsonSerializer.Serialize(handles);
        *result = (void*)Marshal.StringToCoTaskMemUTF8(json);
        return handles.Count;
    }

    private static unsafe int GetEntityComponentData(void** args, void** result)
    {
        var entityId = *(ulong*)args[0];
        if (World.Current is null) return 0;

        var components = new Dictionary<string, Dictionary<string, object>>();
        foreach (var type in World.Current.GetManagedComponentTypes(entityId))
        {
            if (NativeCalls.Native_ComponentExists(entityId, type)) continue;
            if (!World.Current.TryGetComponent(entityId, type, out var component)) continue;

            var fields = new Dictionary<string, object>();
            foreach (var field in type.GetFields(BindingFlags.Public | BindingFlags.Instance))
            {
                try { fields[field.Name] = field.GetValue(component); }
                catch { }
            }
            components[type.FullName ?? type.Name] = fields;
        }

        *result = (void*)Marshal.StringToCoTaskMemUTF8(JsonSerializer.Serialize(components, FieldSerializerOptions));
        return components.Count;
    }

    private static unsafe int SetEntityComponentData(void** args)
    {
        var entityId = *(ulong*)args[0];
        var fullName = Marshal.PtrToStringUTF8((IntPtr)args[1]);
        var fieldsJson = Marshal.PtrToStringUTF8((IntPtr)args[2]);
        if (World.Current is null || string.IsNullOrWhiteSpace(fieldsJson)) return 0;

        var type = FindComponentType(fullName);
        if (type is null || !World.Current.TryGetComponent(entityId, type, out var component)) return 0;

        try
        {
            var fields = JsonSerializer.Deserialize<Dictionary<string, JsonElement>>(fieldsJson);
            if (fields is null) return 0;
            foreach (var (fieldName, jsonValue) in fields)
            {
                var field = type.GetField(fieldName, BindingFlags.Public | BindingFlags.Instance);
                if (field is null || field.IsInitOnly) continue;
                try
                {
                    var converted = JsonSerializer.Deserialize(jsonValue.GetRawText(), field.FieldType, FieldSerializerOptions);
                    if (converted is not null && converted.GetType() == field.FieldType)
                        field.SetValue(component, converted);
                }
                catch { }
            }
            return 1;
        }
        catch { return 0; }
    }

    private static unsafe int AddEntityComponent(void** args)
    {
        var entityId = *(ulong*)args[0];
        var fullName = Marshal.PtrToStringUTF8((IntPtr)args[1]);

        if (World.Current is null || string.IsNullOrWhiteSpace(fullName))
            return 0;

        Type componentType = FindComponentType(fullName);

        if (componentType is null || !typeof(CakeComponent).IsAssignableFrom(componentType))
            return 0;

        if (Activator.CreateInstance(componentType) is not CakeComponent component)
            return 0;

        var entity = new Entity(entityId);
        var generic = typeof(Entity).GetMethods(BindingFlags.Instance | BindingFlags.Public)
            .FirstOrDefault(m => m.Name == "AddComponent" && m.IsGenericMethodDefinition && m.GetParameters().Length == 1);
        if (generic is null) return 0;

        var closed = generic.MakeGenericMethod(componentType);
        closed.Invoke(entity, new object[] { component });
        return 1;
    }

    private static unsafe int RemoveEntityComponent(void** args)
    {
        var entityId = *(ulong*)args[0];
        var fullName = Marshal.PtrToStringUTF8((IntPtr)args[1]);
        if (World.Current is null || string.IsNullOrWhiteSpace(fullName))
            return 0;

        var type = FindComponentType(fullName);
        if (type is null || !typeof(CakeComponent).IsAssignableFrom(type))
            return 0;

        var entity = new Entity(entityId);
        var generic = typeof(Entity).GetMethods(BindingFlags.Instance | BindingFlags.Public)
            .FirstOrDefault(m => m.Name == "RemoveComponent" && m.IsGenericMethodDefinition && m.GetParameters().Length == 0);
        if (generic is null) return 0;

        var closed = generic.MakeGenericMethod(type);
        closed.Invoke(entity, null);

        if (EntityComponentHandles.TryGetValue(entityId, out var trackedHandles))
        {
            for (var i = trackedHandles.Count - 1; i >= 0; i--)
            {
                if (InstanceTypeCache.TryGetValue(trackedHandles[i], out var trackedType) && trackedType == type)
                {
                    ObjectRegistry.Remove(trackedHandles[i]);
                    InstanceTypeCache.Remove(trackedHandles[i]);
                    trackedHandles.RemoveAt(i);
                }
            }
        }
        return 1;
    }

    private static unsafe int RemoveEntity(void** args)
    {
        var entityId = *(ulong*)args[0];
        World.Current?.RemoveEntityLocal(entityId);
        RemoveEntityComponentHandles(entityId);
        return 1;
    }
}
