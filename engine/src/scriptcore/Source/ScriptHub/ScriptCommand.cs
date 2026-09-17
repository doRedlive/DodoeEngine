namespace GreenCake;

internal enum ScriptCommand : ushort
{
    None = 0,

    LoadAppAssembly = 1,
    UnloadApp,
    ResetState,
    ScanTypes,
    CreateInstance,

    InvokeStart,
    InvokeUpdate,
    InvokeFixedUpdate,
    InvokeFinalize,

    GetField,
    SetField,
    Snapshot,
    Restore,

    GcCollect,
    GcInfo,

    GetEntityComponents,
    GetEntityComponentData,
    SetEntityComponentData,
    AddEntityComponent,
    RemoveEntityComponent,
    RemoveEntity,

    RegisterNatives,
    InputActionEvent,
    ListToolActions,
    InvokeToolAction,

    SrpCreatePipeline,
    SrpShutdownPipeline,
    SrpGetFeatureCount,
    SrpGetFeature,
    SrpFeatureInitialize,
    SrpFeatureOnResize,
    SrpFeatureDispose,
    SrpFeatureAddPasses,
    SrpExecute,
    SrpClearExecuteCallbacks,
}
