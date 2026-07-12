namespace GreenCake;

public class Debug
{
    public static void Log(string message)
    {
        NativeCalls.Native_Log(message);
    }

    public static void LogError(string message)
    {
        NativeCalls.Native_Log($"[Error] {message}");
    }

    public static void LogWarning(string message)
    {
        NativeCalls.Native_Log($"[Warning] {message}");
    }
}
