namespace GreenCake;

internal static class SystemDispatcher
{
    public static void InvalidateCache()
    {
        CakeSystemScheduler.InvalidateCache();
    }

    public static void OnCreate()
    {
        Object.BeginValidationFrame();
        CakeSystemScheduler.ExecuteOnCreate();
    }

    public static void OnUpdate()
    {
        Object.BeginValidationFrame();
        CakeSystemScheduler.ExecuteOnUpdate();
    }

    public static void OnFixedUpdate()
    {
        Object.BeginValidationFrame();
        CakeSystemScheduler.ExecuteOnFixedUpdate();
    }

    public static void OnDestroy()
    {
        Object.BeginValidationFrame();
        CakeSystemScheduler.ExecuteOnDestroy();
    }
}
