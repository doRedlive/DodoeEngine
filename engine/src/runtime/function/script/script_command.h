// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    enum class ScriptCommand : UInt16 {
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
    };

    using ScriptCallFn = int (*)(ScriptCommand command, void** args, void** result);
    using ScriptLifecycleFn = void (*)();

} // dodoe
