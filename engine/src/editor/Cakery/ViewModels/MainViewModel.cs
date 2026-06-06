// do@Redlive
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Cakery.Services;

namespace Cakery.ViewModels;

public partial class MainViewModel : ViewModelBase
{
    public HierarchyViewModel Hierarchy { get; } = new();
    public InspectorViewModel Inspector { get; } = new();
    public ConsoleViewModel Console { get; } = new();
    public ProjectViewModel Project { get; } = new();
    public ProjectManagerViewModel ProjectManager { get; } = new();

    [ObservableProperty] private string _title = "Cakery Editor";
    [ObservableProperty] private string _statusText = "Ready";
    [ObservableProperty] private string _fpsDisplay = "";
    [ObservableProperty] private bool _isPlaying;
    [ObservableProperty] private bool _isPaused;
    [ObservableProperty] private bool _workspaceActive;

    private DateTime _lastFrame = DateTime.Now;
    private int _frameCount;

    public void Initialize()
    {
        ProjectManager.RequestEnterEditor += EnterWorkspace;
    }

    public void EnterWorkspace()
    {
        EditorEngine.Initialize();
        Console.Info("Editor", $"Engine v{EditorEngine.Version}");

        WorkspaceActive = true;
        Hierarchy.RefreshCommand.Execute(null);
        Project.RefreshCommand.Execute(null);
        StatusText = "Workspace ready";
    }

    public void Shutdown()
    {
        EditorEngine.Shutdown();
        Console.Info("Editor", "Engine shut down");
    }

    public void UpdateFps()
    {
        _frameCount++;
        var now = DateTime.Now;
        var dt = (now - _lastFrame).TotalSeconds;
        if (dt >= 0.5)
        {
            FpsDisplay = $"{_frameCount / dt:F0} FPS";
            _frameCount = 0;
            _lastFrame = now;
        }
    }

    [RelayCommand] public void Play()
    {
        if (!IsPlaying) { EditorEngine.WorldSetState(1); IsPlaying = true; IsPaused = false; StatusText = "Playing"; }
    }

    [RelayCommand] public void Pause()
    {
        if (IsPlaying) { EditorEngine.WorldSetState(2); IsPaused = !IsPaused; StatusText = IsPaused ? "Paused" : "Playing"; }
    }

    [RelayCommand] public void Stop()
    {
        if (IsPlaying) { EditorEngine.WorldSetState(0); IsPlaying = false; IsPaused = false; StatusText = "Stopped"; }
    }
}
