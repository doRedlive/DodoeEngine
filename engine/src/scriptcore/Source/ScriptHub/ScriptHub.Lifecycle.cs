namespace GreenCake;

using System.Runtime.InteropServices;

public static partial class ScriptHub
{
    [UnmanagedCallersOnly(EntryPoint = "InvokeStart", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void InvokeStart()
    {
        SystemDispatcher.OnCreate();
    }

    [UnmanagedCallersOnly(EntryPoint = "InvokeUpdate", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void InvokeUpdate()
    {
        SystemDispatcher.OnUpdate();
    }

    [UnmanagedCallersOnly(EntryPoint = "InvokeFixedUpdate", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void InvokeFixedUpdate()
    {
        SystemDispatcher.OnFixedUpdate();
    }

    [UnmanagedCallersOnly(EntryPoint = "InvokeFinalize", CallConvs = new[] { typeof(CallConvCdecl) })]
    public static void InvokeFinalize()
    {
        SystemDispatcher.OnDestroy();
    }

    private static unsafe int InvokeSystemOnCreate(void** args)
    {
        SystemDispatcher.OnCreate();
        return 1;
    }

    private static unsafe int InvokeSystemOnUpdate(void** args)
    {
        SystemDispatcher.OnUpdate();
        return 1;
    }

    private static unsafe int InvokeSystemOnFixedUpdate(void** args)
    {
        SystemDispatcher.OnFixedUpdate();
        return 1;
    }

    private static unsafe int InvokeSystemOnDestroy(void** args)
    {
        SystemDispatcher.OnDestroy();
        return 1;
    }
}
