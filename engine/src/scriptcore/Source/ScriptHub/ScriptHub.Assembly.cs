namespace GreenCake;

using System;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

public static partial class ScriptHub
{
    private static unsafe int LoadAppAssembly(void** args, void** result)
    {
        if (args == null || result == null)
            return 0;

        var dataPtr = (IntPtr)args[0];
        var size = (int)(nint)args[1];
        var namePtr = (IntPtr)args[2];
        var name = namePtr != IntPtr.Zero
            ? (Marshal.PtrToStringUTF8(namePtr) ?? "AppAssembly")
            : "AppAssembly";

        if (dataPtr == IntPtr.Zero || size <= 0)
            return 0;

        var alc = new AssemblyLoadContext(name, isCollectible: true);
        var bytes = new byte[size];
        Marshal.Copy(dataPtr, bytes, 0, size);

        using var ms = new System.IO.MemoryStream(bytes);
        alc.LoadFromStream(ms);

        var gcHandle = GCHandle.Alloc(alc, GCHandleType.Normal);
        *(IntPtr*)result = GCHandle.ToIntPtr(gcHandle);
        return 1;
    }

    private static unsafe int UnloadApp(void** args)
    {
        var gcHandle = (GCHandle)((IntPtr)args[0]);
        if (!gcHandle.IsAllocated) return 0;

        var alc = (AssemblyLoadContext)gcHandle.Target;
        gcHandle.Free();
        alc?.Unload();
        return 1;
    }

    private static unsafe int ResetState(void** args)
    {
        ObjectRegistry.Clear();
        InstanceTypeCache.Clear();
        World.Reset();
        GameObjectManager.Reset();
        return 1;
    }

    private static unsafe int CollectAndWait(void** args)
    {
        for (int i = 0; i < 3; i++)
        {
            GC.Collect(GC.MaxGeneration, GCCollectionMode.Forced, true, true);
            GC.WaitForPendingFinalizers();
        }
        return 1;
    }
}
