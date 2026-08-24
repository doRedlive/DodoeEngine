namespace GreenCake;

using System;

public enum InputActionValueType
{
    Button = 0,
    Axis1D = 1,
    Axis2D = 2,
}

public enum InputActionPhase
{
    Waiting = 0,
    Started = 1,
    Performed = 2,
    Canceled = 3,
}

public enum InputInteraction
{
    Press = 0,
    Hold = 1,
    Tap = 2,
    MultiTap = 3,
    Chord = 4,
    Toggle = 5,
    Repeat = 6,
}

public enum InputProcessorType
{
    DeadZone = 0,
    Normalize = 1,
    Scale = 2,
    Invert = 3,
    Clamp = 4,
}

public enum GamepadButtonCode
{
    A = 0,
    B = 1,
    X = 2,
    Y = 3,
    LB = 4,
    RB = 5,
    Back = 6,
    Start = 7,
    Guide = 8,
    LeftStick = 9,
    RightStick = 10,
    DpadUp = 11,
    DpadRight = 12,
    DpadDown = 13,
    DpadLeft = 14,
}

public enum GamepadAxisCode
{
    LeftX = 0,
    LeftY = 1,
    RightX = 2,
    RightY = 3,
    LeftTrigger = 4,
    RightTrigger = 5,
}

public static class Input
{
    public static bool RegisterActionMap(string mapName, int priority = 0)
    {
        return NativeCalls.Native_InputRegisterActionMap(mapName, priority);
    }

    public static bool UnregisterActionMap(string mapName)
    {
        return NativeCalls.Native_InputUnregisterActionMap(mapName);
    }

    public static bool SetActionMapEnabled(string mapName, bool enabled)
    {
        return NativeCalls.Native_InputSetActionMapEnabled(mapName, enabled);
    }

    public static bool SetActionMapConsume(string mapName, bool consume)
    {
        return NativeCalls.Native_InputSetActionMapConsume(mapName, consume);
    }

    public static bool PushContext(string mapName) => NativeCalls.Native_InputPushContext(mapName);

    public static bool PopContext(string mapName) => NativeCalls.Native_InputPopContext(mapName);

    public static bool RegisterAction(string mapName, string actionName, InputActionValueType valueType)
    {
        return NativeCalls.Native_InputRegisterAction(mapName, actionName, (int)valueType);
    }

    public static bool BindKey(string mapName, string actionName, KeyCode key, float scale = 1.0f)
    {
        return NativeCalls.Native_InputBindKey(mapName, actionName, key, scale);
    }

    public static bool BindKey2D(string mapName, string actionName, KeyCode key, float x, float y)
    {
        return NativeCalls.Native_InputBindKey2D(mapName, actionName, key, x, y);
    }

    public static bool BindMouseButton(string mapName, string actionName, MouseCode button, float scale = 1.0f)
    {
        return NativeCalls.Native_InputBindMouseButton(mapName, actionName, button, scale);
    }

    public static bool BindMouseDelta(string mapName, string actionName, float scale = 1.0f)
    {
        return NativeCalls.Native_InputBindMouseDelta(mapName, actionName, scale);
    }

    public static bool BindMouseWheel(string mapName, string actionName, float scale = 1.0f)
    {
        return NativeCalls.Native_InputBindMouseWheel(mapName, actionName, scale);
    }

    public static bool BindGamepadButton(string mapName, string actionName, GamepadButtonCode button, uint deviceId = 0, float scale = 1.0f)
    {
        return NativeCalls.Native_InputBindGamepadButton(mapName, actionName, (int)button, deviceId, scale);
    }

    public static bool BindGamepadAxis(string mapName, string actionName, GamepadAxisCode axis, uint deviceId = 0, float scale = 1.0f)
    {
        return NativeCalls.Native_InputBindGamepadAxis(mapName, actionName, (int)axis, deviceId, scale);
    }

    public static bool BindGamepadStick(string mapName, string actionName, GamepadAxisCode stickAxis, uint deviceId = 0, float scale = 1.0f)
    {
        return NativeCalls.Native_InputBindGamepadStick(mapName, actionName, (int)stickAxis, deviceId, scale);
    }

    public static bool BindComposite(string mapName, string actionName, string partsJson, uint deviceId = 0)
    {
        return NativeCalls.Native_InputBindComposite(mapName, actionName, partsJson, deviceId);
    }

    public static bool SetBindingInteraction(string mapName, string actionName,
        InputInteraction interaction, float holdSeconds = 0.5f)
    {
        return NativeCalls.Native_InputSetBindingInteraction(mapName, actionName, (int)interaction, holdSeconds);
    }

    public static bool SetBindingTapParams(string mapName, string actionName, int bindingIndex, int tapCount, float tapWindowSeconds)
    {
        return NativeCalls.Native_InputSetBindingTapParams(mapName, actionName, bindingIndex, tapCount, tapWindowSeconds);
    }

    public static bool SetBindingRepeatParams(string mapName, string actionName, int bindingIndex, float repeatDelay, float repeatRate)
    {
        return NativeCalls.Native_InputSetBindingRepeatParams(mapName, actionName, bindingIndex, repeatDelay, repeatRate);
    }

    public static bool SetBindingProcessor(string mapName, string actionName, int bindingIndex, InputProcessorType type, float a = 0.0f, float b = 1.0f)
    {
        return NativeCalls.Native_InputSetBindingProcessor(mapName, actionName, bindingIndex, (int)type, a, b);
    }

    public static bool LoadActionAsset(string path) => NativeCalls.Native_InputLoadActionAsset(path);

    public static bool LoadConfigOverrides(string projectPath, string userPath)
    {
        return NativeCalls.Native_InputLoadConfigOverrides(projectPath, userPath);
    }

    public static bool SaveUserConfigOverrides(string userPath)
    {
        return NativeCalls.Native_InputSaveUserConfigOverrides(userPath);
    }

    public static uint FindActionId(string mapName, string actionName) => NativeCalls.Native_InputFindActionId(mapName, actionName);

    public static uint FindActionId(string qualifiedName) => NativeCalls.Native_InputFindActionId(qualifiedName);

    public static bool IsActionDown(string actionName)
    {
        return NativeCalls.Native_InputIsActionDown(actionName);
    }

    public static bool WasActionPressed(string actionName)
    {
        return NativeCalls.Native_InputWasActionPressed(actionName);
    }

    public static bool WasActionReleased(string actionName)
    {
        return NativeCalls.Native_InputWasActionReleased(actionName);
    }

    public static float GetActionAxis(string actionName)
    {
        return NativeCalls.Native_InputGetActionAxis(actionName);
    }

    public static Vector2f GetActionVector2(string actionName)
    {
        return NativeCalls.Native_InputGetActionVector2(actionName);
    }

    public static bool IsActionDown(uint actionId) => NativeCalls.Native_InputIsActionDown(actionId);

    public static bool WasActionPressed(uint actionId) => NativeCalls.Native_InputWasActionPressed(actionId);

    public static bool WasActionReleased(uint actionId) => NativeCalls.Native_InputWasActionReleased(actionId);

    public static float GetActionAxis(uint actionId) => NativeCalls.Native_InputGetActionAxis(actionId);

    public static Vector2f GetActionVector2(uint actionId) => NativeCalls.Native_InputGetActionVector2(actionId);

    public static Vector2f GetMousePosition()
    {
        return NativeCalls.Native_InputGetMousePosition();
    }

    public static Vector2f GetMouseDelta()
    {
        return NativeCalls.Native_InputGetMouseDelta();
    }

    public static Vector2f GetMouseWheel()
    {
        return NativeCalls.Native_InputGetMouseWheel();
    }

    public static bool IsGamepadConnected(uint deviceId) => NativeCalls.Native_InputIsGamepadConnected(deviceId);

    public static bool IsGamepadButtonDown(uint deviceId, GamepadButtonCode button) => NativeCalls.Native_InputIsGamepadButtonDown(deviceId, (int)button);

    public static bool IsGamepadButtonPressed(uint deviceId, GamepadButtonCode button) => NativeCalls.Native_InputIsGamepadButtonPressed(deviceId, (int)button);

    public static bool IsGamepadButtonReleased(uint deviceId, GamepadButtonCode button) => NativeCalls.Native_InputIsGamepadButtonReleased(deviceId, (int)button);

    public static float GetGamepadAxis(uint deviceId, GamepadAxisCode axis) => NativeCalls.Native_InputGetGamepadAxis(deviceId, (int)axis);

    public static ulong Subscribe(string actionName, InputActionPhase phase, Action<InputActionEvent> callback)
    {
        ulong token = NativeCalls.Native_InputSubscribe(actionName, (int)phase);
        uint actionId = FindActionId(actionName);
        InputEventHub.Register(token, actionId, phase, callback);
        return token;
    }

    public static ulong Subscribe(uint actionId, InputActionPhase phase, Action<InputActionEvent> callback)
    {
        ulong token = NativeCalls.Native_InputSubscribe(actionId, (int)phase);
        InputEventHub.Register(token, actionId, phase, callback);
        return token;
    }

    public static void Unsubscribe(ulong subscriptionId)
    {
        NativeCalls.Native_InputUnsubscribe(subscriptionId);
        InputEventHub.Unsubscribe(subscriptionId);
    }

    public static bool SetBindingOverride(string mapName, string actionName, int bindingIndex, string bindingJson)
    {
        return NativeCalls.Native_InputSetBindingOverride(mapName, actionName, bindingIndex, bindingJson);
    }

    public static bool ClearBindingOverride(string mapName, string actionName, int bindingIndex)
    {
        return NativeCalls.Native_InputClearBindingOverride(mapName, actionName, bindingIndex);
    }

    public static bool BeginRebindSession(string mapName, string actionName, int bindingIndex)
    {
        return NativeCalls.Native_InputBeginRebindSession(mapName, actionName, bindingIndex);
    }

    public static void CancelRebindSession() => NativeCalls.Native_InputCancelRebindSession();

    public static bool IsRebindSessionActive() => NativeCalls.Native_InputIsRebindSessionActive();
}
