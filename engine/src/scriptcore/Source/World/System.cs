namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Reflection;

[AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
public sealed class ReadsAttribute : Attribute
{
    public Type[] Types { get; }
    public ReadsAttribute(params Type[] types) => Types = types;
}

[AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
public sealed class WritesAttribute : Attribute
{
    public Type[] Types { get; }
    public WritesAttribute(params Type[] types) => Types = types;
}

[AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
public sealed class ReadsAttribute<T> : Attribute where T : CakeComponent { }

[AttributeUsage(AttributeTargets.Class, AllowMultiple = true)]
public sealed class WritesAttribute<T> : Attribute where T : CakeComponent { }

public readonly struct CakeSystemAccess
{
    public Type[] Reads { get; init; }
    public Type[] Writes { get; init; }
    public bool Structural { get; init; }
}

public class CakeSystem
{
    protected World World { get; private set; } = World.Current;

    public virtual void OnCreate() { }
    public virtual void OnUpdate() { }
    public virtual void OnDestroy() { }

    public virtual CakeSystemAccess GetAccess() => default;
}

internal static class CakeSystemAccessCache
{
    private static readonly Dictionary<Type, CakeSystemAccess> _cache = new();

    public static CakeSystemAccess Resolve(CakeSystem system)
    {
        var type = system.GetType();
        if (_cache.TryGetValue(type, out var cached))
            return cached;

        var access = system.GetAccess();
        if (access.Reads != null || access.Writes != null)
        {
            _cache[type] = access;
            return access;
        }

        var reads = new List<Type>();
        var writes = new List<Type>();

        foreach (var attr in type.GetCustomAttributes())
        {
            var attrType = attr.GetType();
            if (attrType.IsGenericType)
            {
                var genDef = attrType.GetGenericTypeDefinition();
                if (genDef == typeof(ReadsAttribute<>))
                    reads.Add(attrType.GetGenericArguments()[0]);
                else if (genDef == typeof(WritesAttribute<>))
                    writes.Add(attrType.GetGenericArguments()[0]);
            }
            else if (attr is ReadsAttribute r)
            {
                reads.AddRange(r.Types);
            }
            else if (attr is WritesAttribute w)
            {
                writes.AddRange(w.Types);
            }
        }

        access = new CakeSystemAccess
        {
            Reads = reads.ToArray(),
            Writes = writes.ToArray(),
        };
        _cache[type] = access;
        return access;
    }
}
