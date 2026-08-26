namespace GreenCake;

internal static class SystemDispatcher
{
    public static void InvalidateCache()
    {
        CakeSystemScheduler.InvalidateCache();
    }

    public static void OnCreate()
    {
        CakeSystemScheduler.ExecuteOnCreate();
    }

    public static void OnUpdate()
    {
        CakeSystemScheduler.ExecuteOnUpdate();
    }

    public static void OnFixedUpdate()
    {
        CakeSystemScheduler.ExecuteOnFixedUpdate();
    }

    public static void OnDestroy()
    {
        CakeSystemScheduler.ExecuteOnDestroy();
    }
}
