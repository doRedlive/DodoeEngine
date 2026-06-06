// do@Redlive
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using System.IO;

namespace Cakery.ViewModels;

public partial class ProjectViewModel : ObservableObject
{
    [ObservableProperty] private List<FileEntry> _files = new();
    [ObservableProperty] private FileEntry? _selectedFile;
    [ObservableProperty] private string _currentPath = "";
    [ObservableProperty] private string _basePath = "";

    [RelayCommand]
    public void Refresh()
    {
        Files.Clear();
        if (!Directory.Exists(CurrentPath))
        {
            // Fallback to engine resource path
            var resPath = Path.Combine(Directory.GetCurrentDirectory(), "engine", "res");
            if (Directory.Exists(resPath))
            {
                CurrentPath = resPath;
                BasePath = resPath;
            }
            else return;
        }

        try
        {
            foreach (var dir in Directory.GetDirectories(CurrentPath))
            {
                Files.Add(new FileEntry
                {
                    Name = Path.GetFileName(dir),
                    FullPath = dir,
                    IsDirectory = true
                });
            }
            foreach (var file in Directory.GetFiles(CurrentPath))
            {
                Files.Add(new FileEntry
                {
                    Name = Path.GetFileName(file),
                    FullPath = file,
                    IsDirectory = false
                });
            }
        }
        catch { /* access denied */ }
    }

    [RelayCommand]
    public void NavigateInto(FileEntry? entry)
    {
        if (entry == null || !entry.IsDirectory) return;
        CurrentPath = entry.FullPath;
        Refresh();
    }

    [RelayCommand]
    public void NavigateUp()
    {
        var parent = Path.GetDirectoryName(CurrentPath);
        if (parent != null && (BasePath == "" || parent.StartsWith(BasePath)))
        {
            CurrentPath = parent;
            Refresh();
        }
    }

    [RelayCommand]
    public void OpenSelected()
    {
        if (SelectedFile == null || SelectedFile.IsDirectory) return;
        // Open with default program
        try { System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
        {
            FileName = SelectedFile.FullPath,
            UseShellExecute = true
        }); } catch { }
    }
}

public class FileEntry
{
    public string Name { get; set; } = "";
    public string FullPath { get; set; } = "";
    public bool IsDirectory { get; set; }
}
