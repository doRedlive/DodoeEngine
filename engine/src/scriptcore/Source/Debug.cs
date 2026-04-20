namespace GreenCake;

public class Debug
{
    public static void Log(string message)
    {
        InternalCalls.Native_Log(message);
    }
}
