namespace GreenCake;

using System;
using System.Collections.Generic;

public class Object
{
    private static readonly Dictionary<int, Object> s_instanceCache = new();
    private static readonly Dictionary<string, Func<int, int, Object>> s_factories = new();
    private static readonly Dictionary<Type, string> s_managedToNative = new();

    public int InstanceID { get; }
    public int Generation { get; }

    internal Object(int instanceID, int generation = 1)
    {
        InstanceID = instanceID;
        Generation = generation;
    }

    public bool IsValid => InstanceID != 0 && NativeCalls.Native_ObjectIsAlive(InstanceID, Generation);

    public static void RegisterType<T>(string nativeTypeName, Func<int, int, Object> factory) where T : Object
    {
        s_managedToNative[typeof(T)] = nativeTypeName;
        s_factories[nativeTypeName] = factory;
    }

    public static string? GetNativeTypeName(Type managedType)
    {
        s_managedToNative.TryGetValue(managedType, out var name);
        return name;
    }

    public override bool Equals(object? obj) =>
        obj is Object other && InstanceID == other.InstanceID && Generation == other.Generation;

    public override int GetHashCode() => HashCode.Combine(InstanceID, Generation);

    public static bool operator ==(Object? a, Object? b) =>
        ReferenceEquals(a, b) || (a is not null && b is not null && a.InstanceID == b.InstanceID && a.Generation == b.Generation);

    public static bool operator !=(Object? a, Object? b) => !(a == b);

    public static Object? FindObjectFromInstanceID(int instanceID) => GetOrCreate(instanceID);

    public static T? FindObjectFromInstanceID<T>(int instanceID) where T : Object
    {
        return GetOrCreate(instanceID) as T;
    }

    internal static Object? GetOrCreate(int instanceID)
    {
        if (instanceID == 0) {
            return null;
        }

        if (s_instanceCache.TryGetValue(instanceID, out var existing)) {
            if (existing.IsValid) {
                return existing;
            }
            s_instanceCache.Remove(instanceID);
        }

        var generation = NativeCalls.Native_ObjectGeneration(instanceID);
        if (generation == 0) {
            return null;
        }

        var nativeTypeName = NativeCalls.Native_ObjectGetTypeName(instanceID);
        if (string.IsNullOrEmpty(nativeTypeName)) {
            return null;
        }

        if (!s_factories.TryGetValue(nativeTypeName, out var factory)) {
            throw new InvalidOperationException(
                $"Unknown native object type '{nativeTypeName}': no managed factory registered. " +
                "Call Object.RegisterType<T> in the wrapper's static constructor.");
        }

        var created = factory(instanceID, generation);
        s_instanceCache[instanceID] = created;
        return created;
    }
}
