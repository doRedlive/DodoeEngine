namespace GreenCake;

public class Debug
{
    public static void Log(string message)
    {
        InternalCalls.Native_Log(message);
    }

    public static void LogError(string message)
    {
        InternalCalls.Native_Log($"[Error] {message}");
    }

    public static void LogWarning(string message)
    {
        InternalCalls.Native_Log($"[Warning] {message}");
    }
}
