using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Cakery.ViewModels;

namespace Cakery.Views;

public partial class ProjectManagerView : UserControl
{
    public ProjectManagerView() => InitializeComponent();

    // ---- Folder / file pickers (require TopLevel reference, so live in code-behind) ----

    /// Browse button in the "New Project" dialog.
    public async void BrowseNewLocation_Click(object? sender, RoutedEventArgs e)
    {
        if (DataContext is not ProjectManagerViewModel vm) return;
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel is null) return;

        var folders = await topLevel.StorageProvider.OpenFolderPickerAsync(
            new FolderPickerOpenOptions { Title = "Select Project Location" });

        if (folders.Count > 0)
            vm.SetNewLocation(folders[0].Path.LocalPath);
    }

    /// Scan button — recursively finds .doproj files in a chosen folder.
    public async void ScanFolder_Click(object? sender, RoutedEventArgs e)
    {
        if (DataContext is not ProjectManagerViewModel vm) return;
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel is null) return;

        var folders = await topLevel.StorageProvider.OpenFolderPickerAsync(
            new FolderPickerOpenOptions { Title = "Select Folder to Scan for Projects" });

        if (folders.Count > 0)
            await vm.ScanDirectory(folders[0].Path.LocalPath);
    }

    /// Import button — picks a single .doproj file.
    public async void ImportFile_Click(object? sender, RoutedEventArgs e)
    {
        if (DataContext is not ProjectManagerViewModel vm) return;
        var topLevel = TopLevel.GetTopLevel(this);
        if (topLevel is null) return;

        var files = await topLevel.StorageProvider.OpenFilePickerAsync(
            new FilePickerOpenOptions
            {
                Title = "Import .doproj File",
                FileTypeFilter = new[]
                {
                    new FilePickerFileType("Dodoe Project")
                    {
                        Patterns = new[] { "*.doproj" }
                    }
                }
            });

        if (files.Count > 0)
            vm.ImportProject(files[0].Path.LocalPath);
    }
}
