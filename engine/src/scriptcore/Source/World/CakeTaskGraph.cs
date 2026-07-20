namespace GreenCake;

using System;
using System.Collections.Generic;

internal class CakeTaskGraph
{
    public List<CakeSystem[]> Levels { get; private set; } = new();

    public void Build(CakeSystem[] systems)
    {
        Levels.Clear();

        if (systems == null || systems.Length == 0)
            return;

        int n = systems.Length;
        var inDegree = new int[n];
        var adj = new List<int>[n];
        for (int i = 0; i < n; i++)
            adj[i] = new List<int>();

        var producerByType = new Dictionary<Type, int>();
        var readersByType = new Dictionary<Type, List<int>>();

        for (int i = 0; i < n; i++)
        {
            var access = CakeSystemAccessCache.Resolve(systems[i]);
            var reads = access.Reads ?? Array.Empty<Type>();
            var writes = access.Writes ?? Array.Empty<Type>();

            foreach (var type in writes)
            {
                if (producerByType.TryGetValue(type, out int producerIndex))
                {
                    adj[producerIndex].Add(i);
                    inDegree[i]++;
                }

                if (readersByType.TryGetValue(type, out var readerList))
                {
                    foreach (int readerIndex in readerList)
                    {
                        if (readerIndex != i)
                        {
                            adj[readerIndex].Add(i);
                            inDegree[i]++;
                        }
                    }
                    readersByType.Remove(type);
                }

                producerByType[type] = i;
            }

            foreach (var type in reads)
            {
                if (producerByType.TryGetValue(type, out int producerIndex))
                {
                    adj[producerIndex].Add(i);
                    inDegree[i]++;
                }

                if (!readersByType.TryGetValue(type, out var readerList))
                {
                    readerList = new List<int>();
                    readersByType[type] = readerList;
                }
                readerList.Add(i);
            }
        }

        var queue = new Queue<int>();
        for (int i = 0; i < n; i++)
        {
            if (inDegree[i] == 0)
                queue.Enqueue(i);
        }

        var levelCount = new int[n];
        int maxLevel = 0;

        while (queue.Count > 0)
        {
            int u = queue.Dequeue();
            int currentLevel = levelCount[u];

            foreach (int v in adj[u])
            {
                inDegree[v]--;
                if (inDegree[v] == 0)
                {
                    levelCount[v] = currentLevel + 1;
                    if (levelCount[v] > maxLevel)
                        maxLevel = levelCount[v];
                    queue.Enqueue(v);
                }
            }
        }

        var levelLists = new List<CakeSystem>[maxLevel + 1];
        for (int i = 0; i <= maxLevel; i++)
            levelLists[i] = new List<CakeSystem>();

        for (int i = 0; i < n; i++)
            levelLists[levelCount[i]].Add(systems[i]);

        for (int i = 0; i <= maxLevel; i++)
            Levels.Add(levelLists[i].ToArray());
    }
}
