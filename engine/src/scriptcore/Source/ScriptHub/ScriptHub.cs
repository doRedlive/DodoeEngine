namespace GreenCake;

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text.Json;

public static partial class ScriptHub
{
    internal static readonly Dictionary<long, object> ObjectRegistry = new();
    internal static readonly JsonSerializerOptions FieldSerializerOptions = new() { IncludeFields = true };
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
            "invoke_fixed_update"   => InvokeSystemOnFixedUpdate(args),
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
            "get_entity_component_data" => GetEntityComponentData(args, result),
            "set_entity_component_data" => SetEntityComponentData(args),
            "add_entity_component"  => AddEntityComponent(args),
            "remove_entity_component" => RemoveEntityComponent(args),
            "remove_entity"         => RemoveEntity(args),
            "register_natives"      => RegisterNatives(args),
            "input_action_event"    => DispatchInputEvent(args),
            _ => -1
        };
    }

    private static unsafe int DispatchInputEvent(void** args)
    {
        uint actionId = args[0] == null ? 0u : *(uint*)args[0];
        int phase = args[1] == null ? 0 : *(int*)args[1];
        int valueType = args[2] == null ? 0 : *(int*)args[2];
        int boolValue = args[3] == null ? 0 : *(int*)args[3];
        float v0 = args[4] == null ? 0.0f : *(float*)args[4];
        float v1 = args[5] == null ? 0.0f : *(float*)args[5];
        InputEventHub.Dispatch(actionId, phase, valueType, boolValue, v0, v1);
        return 0;
    }
}
