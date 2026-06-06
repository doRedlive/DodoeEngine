// do@Redlive
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Cakery.Models;
using System.Collections.ObjectModel;

namespace Cakery.ViewModels;

public partial class ConsoleViewModel : ObservableObject
{
    [ObservableProperty] private ObservableCollection<LogEntry> _entries = new();
    [ObservableProperty] private string _searchText = "";
    [ObservableProperty] private int _severityFilter; // 0=All, 1=Trace, 2=Debug, 3=Info, 4=Warn, 5=Error, 6=Critical
    [ObservableProperty] private bool _autoScroll;
    [ObservableProperty] private bool _collapseRepeats = true;
    [ObservableProperty] private bool _clearOnPlay;
    [ObservableProperty] private int _errorCount;
    [ObservableProperty] private int _warnCount;

    [RelayCommand] public void Clear()
    {
        Entries.Clear();
        ErrorCount = 0;
        WarnCount = 0;
    }

    [RelayCommand]
    public void SetFilter(int level)
    {
        SeverityFilter = level;
    }

    public void Log(LogLevel level, string message)
    {
        var entry = new LogEntry
        {
            Level = level,
            Message = message,
            Timestamp = DateTime.Now,
            Sequence = Entries.Count
        };

        if (CollapseRepeats && Entries.Count > 0)
        {
            var last = Entries[^1];
            if (last.Level == level && last.Message == message)
            {
                last.RepeatCount++;
                return;
            }
        }

        Entries.Add(entry);
        if (level >= LogLevel.Error) ErrorCount++;
        else if (level == LogLevel.Warn) WarnCount++;

        while (Entries.Count > 1000)
            Entries.RemoveAt(0);
    }

    public void Info(string src, string msg) => Log(LogLevel.Info, msg);
    public void Warn(string src, string msg) => Log(LogLevel.Warn, msg);
    public void Error(string src, string msg) => Log(LogLevel.Error, msg);
}
