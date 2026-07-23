namespace GreenCake;

using System;
using System.Collections.Generic;

internal static class ComponentManager
{
    private static readonly Dictionary<Type, bool> _isNativeCache = new();

    public static bool IsNative(Type t)
    {
        if (_isNativeCache.TryGetValue(t, out var cached))
            return cached;
        var result = typeof(NativeComponent).IsAssignableFrom(t);
        _isNativeCache[t] = result;
        return result;
    }

    public static T Add<T>(Entity e) where T : CakeComponent, new()
    {
        var type = typeof(T);

        if (IsNative(type))
        {
            if (NativeCalls.Native_EntityHasComponent(e.ID, type))
                return Get<T>(e);
            NativeCalls.Native_EntityAddComponent(e.ID, type);
            return (T)(object)NativeProxyFactory.Create(type, e);
        }

        if (ManagedComponentStore.Has<T>(e.ID))
            return ManagedComponentStore.Get<T>(e.ID);

        var instance = new T();
        instance.Entity = e;
        ManagedComponentStore.AddOrReplace(e.ID, instance);

        if (instance is CakeBehaviour mb)
        {
            var scene = SceneManager.ActiveScene;
            if (scene != null)
            {
                var go = scene.FindByID(e.ID);
                mb.GameObject = go;
                scene.QueueAwake(mb);
            }
        }

        return instance;
    }

    public static T Get<T>(Entity e) where T : CakeComponent
    {
        if (IsNative(typeof(T)))
        {
            if (!NativeCalls.Native_EntityHasComponent(e.ID, typeof(T)))
                return null;
            return (T)(object)NativeProxyFactory.Create(typeof(T), e);
        }

        if (ManagedComponentStore.TryGet<T>(e.ID, out var component))
            return component;
        return null;
    }

    public static bool Has<T>(Entity e) where T : CakeComponent
    {
        if (IsNative(typeof(T)))
            return NativeCalls.Native_EntityHasComponent(e.ID, typeof(T));

        return ManagedComponentStore.Has<T>(e.ID);
    }

    public static void Remove<T>(Entity e) where T : CakeComponent
    {
        var type = typeof(T);

        if (IsNative(type))
        {
            if (NativeCalls.Native_EntityHasComponent(e.ID, type))
                NativeCalls.Native_EntityRemoveComponent(e.ID, type);
            return;
        }

        if (ManagedComponentStore.TryGet<T>(e.ID, out var component))
        {
            if (component is CakeBehaviour mb)
            {
                if (!mb._destroyed)
                {
                    mb._destroyed = true;
                    try { mb.OnDestroy(); }
                    catch (Exception ex) { Debug.LogError(string.Format("OnDestroy error: {0}", ex)); }
                }
            }
        }

        ManagedComponentStore.Remove<T>(e.ID);
    }

    public static bool TryGetUserComponent<T>(Entity e, out T component) where T : CakeComponent
    {
        component = null;
        if (IsNative(typeof(T)))
            return false;

        return ManagedComponentStore.TryGet<T>(e.ID, out component);
    }
}
