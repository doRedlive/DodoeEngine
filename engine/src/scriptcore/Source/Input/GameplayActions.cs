namespace GreenCake;

using System;

public class GameplayActions : IDisposable
{
    public readonly InputActionReference Move;
    public readonly InputActionReference Jump;
    public readonly InputActionReference Dash;

    public GameplayActions()
    {
        Move = new InputActionReference("Gameplay/Move");
        Jump = new InputActionReference("Gameplay/Jump");
        Dash = new InputActionReference("Gameplay/Dash");
        Enable();
    }

    public void Enable()
    {
        Move.Enable();
        Jump.Enable();
        Dash.Enable();
    }

    public void Dispose()
    {
        Move.Dispose();
        Jump.Dispose();
        Dash.Dispose();
    }
}
