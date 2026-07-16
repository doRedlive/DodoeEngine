namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

public static partial class ScriptHub
{
    internal static readonly Dictionary<long, object> ObjectRegistry = new();
    private static long NextHandle = 1;

    private static readonly Dictionary<string, Type> SystemTypeCache = new();
    private static readonly Dictionary<long, Type> InstanceTypeCache = new();

    [UnmanagedCallersOnly(EntryPoint = "Call", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static unsafe int Call(byte* methodUtf8, IntPtr pArgs, IntPtr pResult)
    {
        void** args = (void**)pArgs.ToPointer();
        void** result = (void**)pResult.ToPointer();
        var method = Marshal.PtrToStringUTF8((IntPtr)methodUtf8);
        return method switch
        {
            "scan_types"            => ScanAssemblyTypes(args, result),
            "create_instance"       => CreateInstance(args, result),
            "invoke_start"          => InvokeSystemOnCreate(args),
            "invoke_update"         => InvokeSystemOnUpdate(args),
            "invoke_finalize"       => InvokeSystemOnDestroy(args),
            "get_field"             => GetField(args, result),
            "set_field"             => SetField(args),
            "snapshot"              => SnapshotAll(args, result),
            "restore"               => RestoreAll(args),
            "load_app_assembly"     => LoadAppAssembly(args, result),
            "unload_app"            => UnloadApp(args),
            "reset_state"           => ResetState(args),
            "gc_collect"            => CollectAndWait(args),
            "get_entity_components" => GetEntityComponents(args, result),
            "add_entity_component"  => AddEntityComponent(args),
            "remove_entity"         => RemoveEntity(args),
            "register_natives"      => RegisterNatives(args),
            _ => -1
        };
    }
}
