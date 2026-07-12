namespace GreenCake;

public static partial class ScriptHub
{
    private static unsafe int RegisterNatives(void** args)
    {
        NativeCalls.Bind((NativeCalls.NativeBindings*)args[0]);
        return 1;
    }
}
