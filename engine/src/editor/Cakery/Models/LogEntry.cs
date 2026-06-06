// do@Redlive
namespace Cakery.Models;

public enum LogLevel { Trace = 0, Debug, Info, Warn, Error, Critical }

public class LogEntry
{
    public DateTime Timestamp { get; set; } = DateTime.Now;
    public LogLevel Level { get; set; }
    public string Message { get; set; } = "";
    public int RepeatCount { get; set; } = 1;
    public int Sequence { get; set; }
}
