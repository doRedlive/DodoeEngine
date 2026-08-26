namespace GreenCake;

public static partial class ScriptHub
{
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
