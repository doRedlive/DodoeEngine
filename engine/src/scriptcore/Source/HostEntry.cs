using System;
using System.Runtime.InteropServices;

namespace GreenCake
{
    public static class HostEntry
    {
        [UnmanagedCallersOnly(EntryPoint = "Initialize")]
        public static int Initialize()
        {
            Console.WriteLine("[GreenCake] HostEntry.Initialize");
            return 0;
        }

        [UnmanagedCallersOnly(EntryPoint = "Update")]
        public static int Update(float deltaTime)
        {
            return 0;
        }

        [UnmanagedCallersOnly(EntryPoint = "Shutdown")]
        public static int Shutdown()
        {
            Console.WriteLine("[GreenCake] HostEntry.Shutdown");
            return 0;
        }
    }
}
