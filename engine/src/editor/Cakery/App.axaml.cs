// do@Redlive
using System.Reflection;
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Cakery.ViewModels;
using Cakery.Views;

namespace Cakery;

public partial class App : Application
{
    private MainViewModel? _mainVM;

    // Walk up from the exe directory until we find the project root
    // (the directory that contains "engine/CMakeLists.txt").
    // This works both for published builds (exe in bin/) and dev builds
    // (exe deep inside engine/src/editor/Cakery/bin/Debug/net10.0/).
    static App()
    {
        var exeDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location)!;
        var candidate = new DirectoryInfo(exeDir);
        while (candidate != null && !File.Exists(Path.Combine(candidate.FullName, "engine", "CMakeLists.txt")))
            candidate = candidate.Parent;
        if (candidate != null)
            Directory.SetCurrentDirectory(candidate.FullName);
    }

    public override void Initialize() => AvaloniaXamlLoader.Load(this);

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            _mainVM = new MainViewModel();
            _mainVM.Initialize();
            // Start with ProjectManager, EnterWorkspace is called when user opens/creates a project

            desktop.MainWindow = new MainWindow { DataContext = _mainVM };
            desktop.Exit += (_, _) => _mainVM.Shutdown();
        }
        base.OnFrameworkInitializationCompleted();
    }
}
