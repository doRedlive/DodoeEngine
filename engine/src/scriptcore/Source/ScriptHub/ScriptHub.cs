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
    public static unsafe int Call(ushort command, IntPtr pArgs, IntPtr pResult)
    {
        void** args = (void**)pArgs.ToPointer();
        void** result = (void**)pResult.ToPointer();
        return (ScriptCommand)command switch
        {
            ScriptCommand.ScanTypes             => ScanAssemblyTypes(args, result),
            ScriptCommand.CreateInstance        => CreateInstance(args, result),
            ScriptCommand.InvokeStart           => InvokeSystemOnCreate(args),
            ScriptCommand.InvokeUpdate          => InvokeSystemOnUpdate(args),
            ScriptCommand.InvokeFixedUpdate     => InvokeSystemOnFixedUpdate(args),
            ScriptCommand.InvokeFinalize        => InvokeSystemOnDestroy(args),
            ScriptCommand.GetField              => GetField(args, result),
            ScriptCommand.SetField              => SetField(args),
            ScriptCommand.Snapshot              => SnapshotAll(args, result),
            ScriptCommand.Restore               => RestoreAll(args),
            ScriptCommand.LoadAppAssembly       => LoadAppAssembly(args, result),
            ScriptCommand.UnloadApp             => UnloadApp(args),
            ScriptCommand.ResetState            => ResetState(args),
            ScriptCommand.GcCollect             => CollectAndWait(args),
            ScriptCommand.GcInfo                => GcInfo(args, result),
            ScriptCommand.GetEntityComponents   => GetEntityComponents(args, result),
            ScriptCommand.GetEntityComponentData => GetEntityComponentData(args, result),
            ScriptCommand.SetEntityComponentData => SetEntityComponentData(args),
            ScriptCommand.AddEntityComponent    => AddEntityComponent(args),
            ScriptCommand.RemoveEntityComponent => RemoveEntityComponent(args),
            ScriptCommand.RemoveEntity          => RemoveEntity(args),
            ScriptCommand.RegisterNatives       => RegisterNatives(args),
            ScriptCommand.InputActionEvent      => DispatchInputEvent(args),
            ScriptCommand.ListToolActions       => ListToolActions(args, result),
            ScriptCommand.InvokeToolAction      => InvokeToolAction(args, result),
            ScriptCommand.SrpCreatePipeline     => SrpCreatePipeline(args, result),
            ScriptCommand.SrpShutdownPipeline   => SrpShutdownPipeline(args),
            ScriptCommand.SrpGetFeatureCount    => SrpGetFeatureCount(args, result),
            ScriptCommand.SrpGetFeature         => SrpGetFeature(args, result),
            ScriptCommand.SrpFeatureInitialize  => SrpFeatureInitialize(args),
            ScriptCommand.SrpFeatureOnResize    => SrpFeatureOnResize(args),
            ScriptCommand.SrpFeatureDispose     => SrpFeatureDispose(args),
            ScriptCommand.SrpFeatureAddPasses   => SrpFeatureAddPasses(args),
            ScriptCommand.SrpExecute            => SrpExecute(args),
            ScriptCommand.SrpClearExecuteCallbacks => SrpClearExecuteCallbacks(args),
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
