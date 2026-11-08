namespace GreenCake;

using System;

public class CakeCommandBuffer
{
    public void DestroyEntity(ulong entityId) => NativeCalls.Native_EntityEnqueueDestroy(entityId);

    public void RemoveComponent<T>(ulong entityId) where T : CakeComponent
    {
        if (ComponentManager.IsNative(typeof(T)))
            NativeCalls.Native_EntityEnqueueRemoveComponent(entityId, ComponentManager.GetNativeTypeName(typeof(T)));
        else
            NativeCalls.Native_EntityEnqueueRemoveManaged(entityId, typeof(T).FullName);
    }
}
