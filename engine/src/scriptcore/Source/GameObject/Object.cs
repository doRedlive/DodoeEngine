namespace GreenCake;

using System.Collections.Generic;

public class Object
{
    private static readonly Dictionary<int, Object> s_instanceCache = new();

    public int InstanceID { get; }

    internal Object(int instanceID) => InstanceID = instanceID;

    public bool IsValid => InstanceID != 0;

    public override bool Equals(object? obj) =>
        obj is Object other && InstanceID == other.InstanceID;

    public override int GetHashCode() => InstanceID;

    public static bool operator ==(Object? a, Object? b) =>
        ReferenceEquals(a, b) || (a is not null && b is not null && a.InstanceID == b.InstanceID);

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
            return existing;
        }

        var nativeTypeName = NativeCalls.Native_ObjectGetTypeName(instanceID);
        if (string.IsNullOrEmpty(nativeTypeName)) {
            return null;
        }

        var created = CreateManagedWrapper(nativeTypeName, instanceID);
        s_instanceCache[instanceID] = created;
        return created;
    }

    private static Object CreateManagedWrapper(string nativeTypeName, int instanceID)
    {
        return nativeTypeName switch
        {
            "Texture" => new Texture(instanceID),
            _ => new Object(instanceID)
        };
    }
}
