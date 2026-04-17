using System;

namespace GreenCake
{
    public class Debug
    {
        public static void Log(string message)
        {
            InternalCalls.NativeLog(message, 0);
        }
    }
}