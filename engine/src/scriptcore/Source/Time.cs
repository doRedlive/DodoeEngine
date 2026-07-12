namespace GreenCake;

public static class Time
{
    public static float DeltaTime => NativeCalls.Native_TimeGetDeltaTime();
}
