namespace GreenCake;

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

public static class PlayerPrefs
{
    private static readonly Dictionary<string, object> _cache = new();
    private static bool _dirty;
    private static string _savePath;

    private static string SavePath
    {
        get
        {
            if (_savePath != null) return _savePath;
            try
            {
                var dir = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                    "GreenCake", "PlayerPrefs");
                Directory.CreateDirectory(dir);
                _savePath = Path.Combine(dir, "playerprefs.json");
            }
            catch { _savePath = ""; }
            return _savePath;
        }
    }

    static PlayerPrefs() { Load(); }

    private static void Load()
    {
        try
        {
            var path = SavePath;
            if (string.IsNullOrEmpty(path) || !File.Exists(path)) return;
            var json = File.ReadAllText(path);
            if (string.IsNullOrWhiteSpace(json)) return;
            var dict = JsonSerializer.Deserialize<Dictionary<string, JsonElement>>(json);
            if (dict == null) return;
            foreach (var kv in dict)
            {
                var el = kv.Value;
                if (el.ValueKind == JsonValueKind.Number)
                    _cache[kv.Key] = el.GetInt32();
                else if (el.ValueKind == JsonValueKind.String)
                    _cache[kv.Key] = el.GetString();
                else if (el.ValueKind == JsonValueKind.True || el.ValueKind == JsonValueKind.False)
                    _cache[kv.Key] = el.GetBoolean() ? 1 : 0;
            }
        }
        catch { }
    }

    public static bool HasKey(string key) => _cache.ContainsKey(key);

    public static void DeleteKey(string key)
    {
        if (_cache.Remove(key)) _dirty = true;
    }

    public static void DeleteAll()
    {
        if (_cache.Count > 0)
        {
            _cache.Clear();
            _dirty = true;
        }
    }

    public static void SetString(string key, string value)
    {
        _cache[key] = value ?? "";
        _dirty = true;
    }

    public static string GetString(string key, string defaultValue = "")
    {
        if (_cache.TryGetValue(key, out var v) && v is string s) return s;
        return defaultValue;
    }

    public static void SetInt(string key, int value)
    {
        _cache[key] = value;
        _dirty = true;
    }

    public static int GetInt(string key, int defaultValue = 0)
    {
        if (_cache.TryGetValue(key, out var v))
        {
            if (v is int i) return i;
            if (v is string s && int.TryParse(s, out var iv)) return iv;
        }
        return defaultValue;
    }

    public static void SetFloat(string key, float value)
    {
        _cache[key] = value.ToString("G9");
        _dirty = true;
    }

    public static float GetFloat(string key, float defaultValue = 0)
    {
        if (_cache.TryGetValue(key, out var v))
        {
            if (v is int i) return i;
            if (v is string s && float.TryParse(s, out var fv)) return fv;
        }
        return defaultValue;
    }

    public static void Save()
    {
        if (!_dirty) return;
        try
        {
            var path = SavePath;
            if (string.IsNullOrEmpty(path)) return;
            var dict = new Dictionary<string, object>();
            foreach (var kv in _cache) dict[kv.Key] = kv.Value;
            var json = JsonSerializer.Serialize(dict, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(path, json);
            _dirty = false;
        }
        catch { }
    }
}
