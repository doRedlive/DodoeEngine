namespace GreenCake;

public static class Time
{
    public static float DeltaTime => InternalCalls.Native_TimeGetDeltaTime();
}
