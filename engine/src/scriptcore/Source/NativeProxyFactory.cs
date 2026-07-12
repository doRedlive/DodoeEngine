namespace GreenCake;

using System;
using System.Collections.Generic;

internal static class NativeProxyFactory
{
    private static readonly Dictionary<Type, Func<Entity, NativeComponent>> _factories = new();

    public static void Register<T>(Func<Entity, T> factory) where T : NativeComponent
    {
        _factories[typeof(T)] = e => factory(e);
    }

    public static T Create<T>(Entity entity) where T : NativeComponent
    {
        if (_factories.TryGetValue(typeof(T), out var factory))
            return (T)factory(entity);

        var instance = Activator.CreateInstance<T>();
        instance.Entity = entity;
        return instance;
    }

    public static NativeComponent Create(Type type, Entity entity)
    {
        if (_factories.TryGetValue(type, out var factory))
            return factory(entity);

        var instance = (NativeComponent)Activator.CreateInstance(type);
        instance.Entity = entity;
        return instance;
    }

    internal static bool IsRegistered<T>() where T : NativeComponent
    {
        return _factories.ContainsKey(typeof(T));
    }
}
