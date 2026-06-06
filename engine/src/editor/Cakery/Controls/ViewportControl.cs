// do@Redlive
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Cakery.Services;

namespace Cakery.Controls;

public class ViewportControl : UserControl
{
    private readonly EditorCamera2D _camera = new();

    public ViewportControl()
    {
        Background = new SolidColorBrush(Colors.Black);
        ClipToBounds = true;
        Focusable = true;

        Content = new TextBlock
        {
            Text = "Scene",
            Foreground = new SolidColorBrush(Colors.DarkGray),
            FontSize = 18,
            HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Center,
            VerticalAlignment = Avalonia.Layout.VerticalAlignment.Center
        };

        PointerPressed += OnPointerPressed;
        PointerReleased += OnPointerReleased;
        PointerMoved += OnPointerMoved;
        PointerWheelChanged += OnPointerWheel;
    }

    private void OnPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        var pt = e.GetCurrentPoint(this);
        int button = pt.Properties.IsMiddleButtonPressed ? 1
                   : pt.Properties.IsRightButtonPressed ? 2 : 0;
        _camera.OnMouseDown(pt.Position.X, pt.Position.Y, button);
    }

    private void OnPointerReleased(object? sender, PointerReleasedEventArgs e)
    {
        int button = e.InitialPressMouseButton == MouseButton.Middle ? 1 : 0;
        _camera.OnMouseUp(button);
    }

    private void OnPointerMoved(object? sender, PointerEventArgs e)
    {
        var pt = e.GetCurrentPoint(this);
        _camera.OnMouseMove(pt.Position.X, pt.Position.Y);
    }

    private void OnPointerWheel(object? sender, PointerWheelEventArgs e)
    {
        _camera.OnScroll(e.Delta.Y);
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnAttachedToVisualTree(e);
        if (VisualRoot is TopLevel tl)
            tl.RequestAnimationFrame(OnFrame);
    }

    private void OnFrame(TimeSpan delta)
    {
        EditorEngine.Tick();
        _camera.SetViewportSize((float)Bounds.Width, (float)Bounds.Height);
        if (VisualRoot is TopLevel tl)
            tl.RequestAnimationFrame(OnFrame);
    }
}
