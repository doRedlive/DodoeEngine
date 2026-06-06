// do@Redlive
// Godot-style project manager — supports Scan, Import, missing-project badges,
// safe "Remove from List" vs dangerous "Delete from Disk", sort options, and
// an empty-state onboarding panel.
using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Serialization;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace Cakery.ViewModels;

public enum SortOption
{
    LastEdited,  // default
    Name,
    Path
}

public partial class ProjectManagerViewModel : ViewModelBase
{
    // ============================================================
    // Observable properties
    // ============================================================
    [ObservableProperty] private List<ProjectEntry> _projects = new();
    [ObservableProperty] private List<ProjectEntry> _filteredProjects = new();
    [ObservableProperty] private ProjectEntry? _selected;
    [ObservableProperty] private string _searchText = "";
    [ObservableProperty] private string _lastOpened = "";
    [ObservableProperty] private string _newName = "NewProject";
    [ObservableProperty] private string _newLocation = "";

    // Popup toggles
    [ObservableProperty] private bool _showNewPopup;
    [ObservableProperty] private bool _showRenamePopup;
    [ObservableProperty] private bool _showRemoveConfirmPopup;
    [ObservableProperty] private bool _showDeleteFromDiskPopup;
    [ObservableProperty] private bool _showRemoveMissingPopup;

    // Computed helpers for binding
    [ObservableProperty] private bool _isAnyProjectSelected;
    [ObservableProperty] private bool _isSelectedProjectMissing;
    [ObservableProperty] private bool _isListEmpty = true;
    [ObservableProperty] private int _totalProjectCount;
    [ObservableProperty] private int _missingProjectCount;
    [ObservableProperty] private bool _hasMissingProjects;

    // Sort
    [ObservableProperty] private SortOption _sortOption = SortOption.LastEdited;
    [ObservableProperty] private int _sortOptionIndex;

    // Rename
    [ObservableProperty] private string _renameText = "";

    public event Action? RequestEnterEditor;

    // ============================================================
    // Config paths
    // ============================================================
    private static readonly string ConfigDir =
        Path.Combine(Directory.GetCurrentDirectory(), "engine", "res", "configs");
    private static readonly string ConfigPath =
        Path.Combine(ConfigDir, "project_manager_config.json");

    public ProjectManagerViewModel()
    {
        SyncDefaultLocation();
        ReadConfig();
    }

    // ============================================================
    // Persistence
    // ============================================================
    private void ReadConfig()
    {
        Projects.Clear();
        try
        {
            if (File.Exists(ConfigPath))
            {
                var json = File.ReadAllText(ConfigPath);
                var data = JsonSerializer.Deserialize<ConfigData>(json);
                if (data != null)
                {
                    Projects = data.Projects ?? new();
                    LastOpened = data.LastOpened ?? "";
                    if (!string.IsNullOrEmpty(data.LastSortOption)
                        && Enum.TryParse<SortOption>(data.LastSortOption, out var s))
                    {
                        SortOption = s;
                        SortOptionIndex = (int)s;
                    }
                }
            }
        }
        catch { }

        // Compute derived fields for every entry
        foreach (var p in Projects)
        {
            p.IsMissing = !File.Exists(p.ProjectFile);
            if (!p.IsMissing)
            {
                p.LastModifiedUnix = new DateTimeOffset(
                    File.GetLastWriteTimeUtc(p.ProjectFile!)).ToUnixTimeSeconds();
                p.ProjectPath = Path.GetDirectoryName(p.ProjectFile!);
            }
            ReadProjectDescription(p);
        }

        RebuildFilter();
        SelectLastOpened();
    }

    private void WriteConfig()
    {
        try
        {
            Directory.CreateDirectory(ConfigDir);
            File.WriteAllText(ConfigPath, JsonSerializer.Serialize(
                new ConfigData
                {
                    Version = 1,
                    Projects = Projects,
                    LastOpened = LastOpened,
                    LastSortOption = SortOption.ToString()
                },
                new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { }
    }

    private static void ReadProjectDescription(ProjectEntry p)
    {
        if (p.IsMissing) return;
        try
        {
            var json = File.ReadAllText(p.ProjectFile!);
            using var doc = JsonDocument.Parse(json);
            if (doc.RootElement.TryGetProperty("Project", out var proj))
            {
                if (proj.TryGetProperty("Description", out var desc))
                    p.Description = desc.GetString();
                if (proj.TryGetProperty("Version", out var ver))
                    p.EngineVersion = ver.GetString();
            }
        }
        catch { }
    }

    // ============================================================
    // Filtering & sorting
    // ============================================================
    partial void OnSearchTextChanged(string value) => RebuildFilter();
    partial void OnSortOptionChanged(SortOption value)
    {
        SortOptionIndex = (int)value;
        RebuildFilter();
    }
    partial void OnSortOptionIndexChanged(int value)
    {
        SortOption = (SortOption)value;
    }

    private void RebuildFilter()
    {
        var q = string.IsNullOrWhiteSpace(SearchText) ? null : SearchText;

        IEnumerable<ProjectEntry> source = Projects;
        if (q != null)
            source = source.Where(p =>
                p.Name.Contains(q, StringComparison.OrdinalIgnoreCase) ||
                (p.ProjectFile?.Contains(q, StringComparison.OrdinalIgnoreCase) ?? false));

        // Pinned always float to top, then apply sort within each group
        var sortFunc = SortOption switch
        {
            SortOption.Name => (Func<ProjectEntry, IComparable>)(p => p.Name),
            SortOption.Path => p => p.ProjectFile ?? "",
            _ => p => -(p.LastModifiedUnix > 0 ? p.LastModifiedUnix : p.LastOpenedUnix)
        };

        var pinned = source.Where(p => p.Pinned).OrderBy(sortFunc);
        var unpinned = source.Where(p => !p.Pinned).OrderBy(sortFunc);

        FilteredProjects = pinned.Concat(unpinned).ToList();

        if (FilteredProjects.Count > 0 && Selected == null)
            Selected = FilteredProjects[0];

        UpdateCounts();
    }

    private void SelectLastOpened()
    {
        if (string.IsNullOrEmpty(LastOpened)) return;
        var found = FilteredProjects.FirstOrDefault(p =>
            string.Equals(p.ProjectFile, LastOpened, StringComparison.OrdinalIgnoreCase));
        if (found != null) Selected = found;
    }

    private void UpdateCounts()
    {
        TotalProjectCount = Projects.Count;
        MissingProjectCount = Projects.Count(p => p.IsMissing);
        HasMissingProjects = MissingProjectCount > 0;
        IsListEmpty = FilteredProjects.Count == 0;
    }

    partial void OnSelectedChanged(ProjectEntry? value)
    {
        IsAnyProjectSelected = value != null;
        IsSelectedProjectMissing = value != null && value.IsMissing;
    }

    // ============================================================
    // Open / Create / Close
    // ============================================================

    [RelayCommand]
    public void OpenProject()
    {
        if (Selected == null || Selected.IsMissing) return;
        if (File.Exists(Selected.ProjectFile))
        {
            MarkOpened(Selected.ProjectFile);
            WriteConfig();
            RequestEnterEditor?.Invoke();
        }
    }

    [RelayCommand] public void ShowNewDialog() => ShowNewPopup = true;

    [RelayCommand]
    public void CreateNewProject()
    {
        if (string.IsNullOrWhiteSpace(NewName) || string.IsNullOrWhiteSpace(NewLocation)) return;
        var fullDir = Path.Combine(NewLocation, NewName);
        try
        {
            Directory.CreateDirectory(Path.Combine(fullDir, "Assets", "Scenes"));
            Directory.CreateDirectory(Path.Combine(fullDir, "Binaries"));
            Directory.CreateDirectory(Path.Combine(fullDir, "Configs"));

            var projFile = Path.Combine(fullDir, $"{NewName}.doproj");
            File.WriteAllText(projFile, JsonSerializer.Serialize(
                new { Project = new { Name = NewName, Version = "1.0" } },
                new JsonSerializerOptions { WriteIndented = true }));

            var entry = new ProjectEntry
            {
                Name = NewName,
                ProjectFile = projFile,
                LastOpenedUnix = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
                LastModifiedUnix = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
                ProjectPath = fullDir,
                IsMissing = false
            };
            Upsert(entry);

            ShowNewPopup = false;
            MarkOpened(projFile);
            WriteConfig();
            RequestEnterEditor?.Invoke();
        }
        catch (Exception ex) { Debug.WriteLine(ex.Message); }
    }

    [RelayCommand] public void CancelNew() => ShowNewPopup = false;

    // ============================================================
    // Scan, Import, Refresh
    // ============================================================

    // Called from code-behind — scans a directory tree for .doproj files
    public async Task ScanDirectory(string folderPath)
    {
        if (string.IsNullOrEmpty(folderPath) || !Directory.Exists(folderPath))
            return;

        var doprojFiles = await Task.Run(() =>
            Directory.GetFiles(folderPath, "*.doproj", SearchOption.AllDirectories));

        int added = 0;
        foreach (var file in doprojFiles)
        {
            var norm = Path.GetFullPath(file);
            if (Projects.Any(p =>
                string.Equals(Path.GetFullPath(p.ProjectFile!), norm, StringComparison.OrdinalIgnoreCase)))
                continue;

            var entry = BuildEntry(norm);
            Projects.Add(entry);
            added++;
        }

        Debug.WriteLine($"Scan: added {added} projects from {folderPath}");
        RebuildFilter();
        WriteConfig();
    }

    // Called from code-behind — adds a single .doproj file to the list
    public void ImportProject(string filePath)
    {
        if (string.IsNullOrEmpty(filePath) || !File.Exists(filePath))
            return;
        if (!filePath.EndsWith(".doproj", StringComparison.OrdinalIgnoreCase))
            return;

        var norm = Path.GetFullPath(filePath);
        if (Projects.Any(p =>
            string.Equals(Path.GetFullPath(p.ProjectFile!), norm, StringComparison.OrdinalIgnoreCase)))
            return;

        var entry = BuildEntry(norm);
        Projects.Add(entry);
        RebuildFilter();
        WriteConfig();
    }

    private ProjectEntry BuildEntry(string absolutePath)
    {
        var entry = new ProjectEntry
        {
            Name = Path.GetFileNameWithoutExtension(absolutePath),
            ProjectFile = absolutePath,
            IsMissing = false,
            LastModifiedUnix = new DateTimeOffset(
                File.GetLastWriteTimeUtc(absolutePath)).ToUnixTimeSeconds(),
            ProjectPath = Path.GetDirectoryName(absolutePath)
        };
        ReadProjectDescription(entry);
        return entry;
    }

    [RelayCommand]
    public void RefreshProjects()
    {
        foreach (var p in Projects)
        {
            p.IsMissing = !File.Exists(p.ProjectFile);
            if (!p.IsMissing)
            {
                p.LastModifiedUnix = new DateTimeOffset(
                    File.GetLastWriteTimeUtc(p.ProjectFile!)).ToUnixTimeSeconds();
                p.ProjectPath = Path.GetDirectoryName(p.ProjectFile);
                ReadProjectDescription(p);
            }
        }
        RebuildFilter();
        WriteConfig();
    }

    // ============================================================
    // Remove Missing
    // ============================================================
    [RelayCommand] public void ShowRemoveMissingDialog() => ShowRemoveMissingPopup = true;
    [RelayCommand]
    public void ConfirmRemoveMissing()
    {
        Projects.RemoveAll(p => p.IsMissing);
        ShowRemoveMissingPopup = false;
        Selected = null;
        RebuildFilter();
        WriteConfig();
    }
    [RelayCommand] public void CancelRemoveMissing() => ShowRemoveMissingPopup = false;

    // ============================================================
    // Rename
    // ============================================================
    [RelayCommand]
    public void ShowRenameDialog()
    {
        if (Selected == null) return;
        RenameText = Selected.Name;
        ShowRenamePopup = true;
    }
    [RelayCommand]
    public void ConfirmRename()
    {
        if (Selected == null || string.IsNullOrWhiteSpace(RenameText)) return;
        Selected.Name = RenameText;
        ShowRenamePopup = false;
        RebuildFilter();
        WriteConfig();
    }
    [RelayCommand] public void CancelRename() => ShowRenamePopup = false;

    // ============================================================
    // Remove from list (safe — no file deletion)
    // ============================================================
    [RelayCommand] public void ShowRemoveFromListDialog() => ShowRemoveConfirmPopup = true;

    [RelayCommand]
    public void ConfirmRemoveFromList()
    {
        if (Selected == null) return;
        Projects.Remove(Selected);
        ShowRemoveConfirmPopup = false;
        Selected = null;
        RebuildFilter();
        WriteConfig();
    }
    [RelayCommand] public void CancelRemoveFromList() => ShowRemoveConfirmPopup = false;

    // ============================================================
    // Delete from disk (dangerous — destroys the project folder)
    // ============================================================
    [RelayCommand] public void ShowDeleteFromDiskDialog() => ShowDeleteFromDiskPopup = true;

    [RelayCommand]
    public void ConfirmDeleteFromDisk()
    {
        if (Selected == null) return;
        try
        {
            var d = Selected.ProjectPath ?? Path.GetDirectoryName(Selected.ProjectFile);
            if (d != null && Directory.Exists(d))
                Directory.Delete(d, true);
        }
        catch { }
        Projects.Remove(Selected);
        ShowDeleteFromDiskPopup = false;
        Selected = null;
        RebuildFilter();
        WriteConfig();
    }
    [RelayCommand] public void CancelDeleteFromDisk() => ShowDeleteFromDiskPopup = false;

    // ============================================================
    // Pin / Show in Explorer
    // ============================================================
    [RelayCommand]
    public void TogglePin()
    {
        if (Selected != null) { Selected.Pinned = !Selected.Pinned; RebuildFilter(); WriteConfig(); }
    }

    [RelayCommand]
    public void ShowInExplorer()
    {
        if (Selected == null) return;
        var dir = Selected.ProjectPath ?? Path.GetDirectoryName(Selected.ProjectFile);
        if (dir != null && Directory.Exists(dir))
        {
            try
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = dir,
                    UseShellExecute = true
                });
            }
            catch { }
        }
    }

    // ============================================================
    // Helpers
    // ============================================================
    private void MarkOpened(string path)
    {
        var norm = Path.GetFullPath(path);
        var existing = Projects.FirstOrDefault(p =>
            string.Equals(Path.GetFullPath(p.ProjectFile!), norm, StringComparison.OrdinalIgnoreCase));
        if (existing != null)
            existing.LastOpenedUnix = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
        else
            Upsert(new ProjectEntry
            {
                Name = Path.GetFileNameWithoutExtension(path),
                ProjectFile = norm,
                LastOpenedUnix = DateTimeOffset.UtcNow.ToUnixTimeSeconds()
            });
        LastOpened = norm;
    }

    private void Upsert(ProjectEntry entry)
    {
        var norm = Path.GetFullPath(entry.ProjectFile!);
        var existing = Projects.FirstOrDefault(p =>
            string.Equals(Path.GetFullPath(p.ProjectFile!), norm, StringComparison.OrdinalIgnoreCase));
        if (existing != null)
        {
            existing.Name = entry.Name;
            existing.LastOpenedUnix = entry.LastOpenedUnix;
            if (entry.LastModifiedUnix > 0) existing.LastModifiedUnix = entry.LastModifiedUnix;
            if (entry.ProjectPath != null) existing.ProjectPath = entry.ProjectPath;
        }
        else Projects.Add(entry);
    }

    /// Called from code-behind to set the location from a folder picker result.
    public void SetNewLocation(string path)
    {
        if (!string.IsNullOrEmpty(path))
            NewLocation = path;
    }

    private void SyncDefaultLocation()
    {
        if (string.IsNullOrEmpty(NewLocation))
            NewLocation = Path.Combine(Environment.GetFolderPath(
                Environment.SpecialFolder.MyDocuments), "DodoeProjects");
    }
}

// ============================================================
// Data models
// ============================================================

public class ProjectEntry
{
    // --- Serialised ---
    public string Name { get; set; } = "";
    public string? ProjectFile { get; set; }
    public bool Pinned { get; set; }
    public long LastOpenedUnix { get; set; }
    public long LastModifiedUnix { get; set; }
    public string? IconPath { get; set; }
    public List<string> Tags { get; set; } = new();
    public string? Description { get; set; }
    public string? EngineVersion { get; set; }

    // --- Computed (not serialised) ---
    [JsonIgnore] public bool IsMissing { get; set; }
    [JsonIgnore] public string? ProjectPath { get; set; }

    // --- Binding helpers (not serialised) ---
    [JsonIgnore] public double RowOpacity => IsMissing ? 0.5 : 1.0;
    [JsonIgnore] public bool ShowStar => Pinned && !IsMissing;
    [JsonIgnore] public string StarColor => Pinned ? "#DAA520" : "#555555";
    [JsonIgnore] public string DisplayDate
    {
        get
        {
            if (IsMissing) return "Missing";
            var ticks = LastModifiedUnix > 0 ? LastModifiedUnix : LastOpenedUnix;
            if (ticks == 0) return "";
            var dt = DateTimeOffset.FromUnixTimeSeconds(ticks);
            var local = dt.LocalDateTime;
            return $"{local.Year}-{local.Month:D2}-{local.Day:D2} {local.Hour:D2}:{local.Minute:D2}";
        }
    }
}

public class ConfigData
{
    public int Version { get; set; } = 1;
    public List<ProjectEntry> Projects { get; set; } = new();
    public string? LastOpened { get; set; }
    public string? LastSortOption { get; set; }
}
