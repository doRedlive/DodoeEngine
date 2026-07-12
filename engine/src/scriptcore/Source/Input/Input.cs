namespace GreenCake;

using System;

public static class Input
{
    public static bool IsKeyDown(KeyCode keycode)
    {
        return NativeCalls.Native_IsKeyDown(keycode);
    }
}
