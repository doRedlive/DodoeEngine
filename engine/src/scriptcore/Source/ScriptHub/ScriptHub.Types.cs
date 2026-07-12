namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Text.Json;

public static partial class ScriptHub
{
    private static IEnumerable<Type> EnumerateTypes(System.Reflection.Assembly asm)
    {
        try
        {
            return asm.GetTypes();
        }
        catch (System.Reflection.ReflectionTypeLoadException ex)
        {
            return ex.Types.Where(t => t != null)!;
        }
    }

    private static unsafe int ScanAssemblyTypes(void** args, void** result)
    {
        var types = new List<object>();

        void scanAssembly(System.Reflection.Assembly asm)
        {
            foreach (var t in EnumerateTypes(asm))
            {
                if (!t.IsPublic || t.IsAbstract)
                    continue;
                types.Add(new
                {
                    ns = t.Namespace ?? "",
                    name = t.Name,
                    baseNs = t.BaseType?.Namespace ?? "",
                    baseName = t.BaseType?.Name ?? ""
                });
            }
        }

        foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
            scanAssembly(asm);

        if (args != null && args[0] != null)
        {
            var gcHandle = (GCHandle)((IntPtr)args[0]);
            if (gcHandle.IsAllocated && gcHandle.Target is AssemblyLoadContext alc)
            {
                foreach (var asm in alc.Assemblies)
                    scanAssembly(asm);
            }
        }

        var json = JsonSerializer.Serialize(types);
        *result = (void*)Marshal.StringToCoTaskMemUTF8(json);
        return types.Count;
    }

    private static unsafe int CreateInstance(void** args, void** result)
    {
        var ns = Marshal.PtrToStringUTF8((IntPtr)args[0]);
        var name = Marshal.PtrToStringUTF8((IntPtr)args[1]);

        Type type = null;
        var fullName = string.IsNullOrEmpty(ns) ? name : $"{ns}.{name}";
        if (SystemTypeCache.TryGetValue(fullName, out type))
        {
            
        }
        else
        {
            type = InstanceTypeCache.Values.FirstOrDefault(t =>
                (string.IsNullOrEmpty(ns) || t.Namespace == ns) && t.Name == name);
        }

        if (type == null)
        {
            foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
            {
                type = asm.GetType(fullName, throwOnError: false, ignoreCase: false);
                if (type != null) break;
                type = EnumerateTypes(asm).FirstOrDefault(t =>
                    (string.IsNullOrEmpty(ns) || t.Namespace == ns) && t.Name == name);
                if (type != null) break;
            }
        }

        if (type == null && AppAlc != null)
        {
            foreach (var asm in AppAlc.Assemblies)
            {
                type = asm.GetType(fullName, throwOnError: false, ignoreCase: false);
                if (type != null) break;
                type = EnumerateTypes(asm).FirstOrDefault(t =>
                    (string.IsNullOrEmpty(ns) || t.Namespace == ns) && t.Name == name);
                if (type != null) break;
            }
        }

        if (type == null) return 0;

        var obj = Activator.CreateInstance(type);
        if (obj == null) return 0;

        var handle = NextHandle++;
        ObjectRegistry[handle] = obj;
        InstanceTypeCache[handle] = type;
        SystemTypeCache[fullName] = type;

        *(long*)result = handle;
        return 1;
    }
}
