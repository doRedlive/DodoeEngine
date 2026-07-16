namespace GreenCake;

using System;
using System.Linq;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

public static partial class ScriptHub
{
    private static AssemblyLoadContext AppAlc;

    private sealed class ScriptAssemblyLoadContext : AssemblyLoadContext
    {
        private readonly AssemblyDependencyResolver _resolver;

        public ScriptAssemblyLoadContext(string assemblyPath, string name)
            : base(name, isCollectible: true)
        {
            _resolver = new AssemblyDependencyResolver(assemblyPath);
        }

        protected override System.Reflection.Assembly Load(System.Reflection.AssemblyName assemblyName)
        {
            var sharedAssembly = AppDomain.CurrentDomain.GetAssemblies()
                .FirstOrDefault(asm => asm.GetName().Name == assemblyName.Name);
            if (sharedAssembly != null)
                return sharedAssembly;

            var path = _resolver.ResolveAssemblyToPath(assemblyName);
            return path != null ? LoadFromAssemblyPath(path) : null;
        }

        protected override IntPtr LoadUnmanagedDll(string unmanagedDllName)
        {
            var path = _resolver.ResolveUnmanagedDllToPath(unmanagedDllName);
            return path != null ? LoadUnmanagedDllFromPath(path) : IntPtr.Zero;
        }
    }

    private static unsafe int LoadAppAssembly(void** args, void** result)
    {
        if (args == null || result == null)
            return 0;

        var dataPtr = (IntPtr)args[0];
        var size = (int)(nint)args[1];
        var namePtr = (IntPtr)args[2];
        var assemblyPath = namePtr != IntPtr.Zero
            ? Marshal.PtrToStringUTF8(namePtr)
            : null;
        
        if (string.IsNullOrWhiteSpace(assemblyPath) || dataPtr == IntPtr.Zero || size <= 0)
            return 0;

        var alc = new ScriptAssemblyLoadContext(assemblyPath, System.IO.Path.GetFileNameWithoutExtension(assemblyPath));
        
        byte[] assemblyBytes = new byte[size];
        Marshal.Copy(dataPtr, assemblyBytes, 0, size);
        
        using var ms = new System.IO.MemoryStream(assemblyBytes);
        alc.LoadFromStream(ms);
        
        AppAlc = alc;

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
        AppAlc = null;
        return 1;
    }

    private static unsafe int ResetState(void** args)
    {
        ObjectRegistry.Clear();
        InstanceTypeCache.Clear();
        SystemTypeCache.Clear();
        SystemDispatcher.InvalidateCache();
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
