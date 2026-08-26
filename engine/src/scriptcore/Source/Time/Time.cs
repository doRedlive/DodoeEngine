namespace GreenCake;

public static class Time
{
    private static float _time;
    private static double _realtime;

    public static float DeltaTime => NativeCalls.Native_TimeGetDeltaTime();

    public static float FixedDeltaTime { get; internal set; } = NativeCalls.Native_Time_GetFixedDeltaTime();

    public static float time => _time;

    public static double realtimeSinceStartup => _realtime;

    public static float timeScale { get; set; } = 1f;

    public static float unscaledDeltaTime => DeltaTime;

    internal static void Tick(float dt)
    {
        _time += dt;
        _realtime += dt;
    }
}
