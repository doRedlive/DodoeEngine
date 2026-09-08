namespace GreenCake;

using System;
using System.Runtime.InteropServices;
using System.Text.Json;

public static partial class ScriptHub
{
    private static unsafe int GcInfo(void** args, void** result)
    {
        var registryCount = ObjectRegistry.Count;
        var instanceCacheCount = InstanceTypeCache.Count;
        ulong entityHandleTotal = 0;
        foreach (var list in EntityComponentHandles)
            entityHandleTotal += (ulong)list.Value.Count;

        var gcInfo = GC.GetGCMemoryInfo();
        var info = new
        {
            heapAllocatedBytes = GC.GetTotalMemory(false),
            heapSizeBytes = (ulong)gcInfo.HeapSizeBytes,
            memoryLoadBytes = (ulong)gcInfo.MemoryLoadBytes,
            gen0Collections = GC.CollectionCount(0),
            gen1Collections = GC.CollectionCount(1),
            gen2Collections = GC.CollectionCount(2),
            assemblyCount = AppDomain.CurrentDomain.GetAssemblies().Length,
            objectRegistryCount = (ulong)registryCount,
            instanceTypeCacheCount = (ulong)instanceCacheCount,
            entityHandleTotal = entityHandleTotal
        };

        *result = (void*)Marshal.StringToCoTaskMemUTF8(JsonSerializer.Serialize(info));
        return 1;
    }
}
