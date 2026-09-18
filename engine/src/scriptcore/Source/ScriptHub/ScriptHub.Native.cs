namespace GreenCake;

public static partial class ScriptHub
{
    private static unsafe int RegisterNatives(void** args)
    {
        NativeCalls.Bind((NativeCalls.NativeBindings*)args[0]);
        return 1;
    }

    private static unsafe int RegisterEditorNatives(void** args)
    {
        if (args == null || args[0] == null)
            return 0;
        GreenCake.Editor.EditorNativeCalls.Bind(
            (GreenCake.Editor.EditorNativeCalls.EditorBindings*)args[0]);
        return 1;
    }
}
