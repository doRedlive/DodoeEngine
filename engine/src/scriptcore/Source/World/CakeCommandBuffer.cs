namespace GreenCake;

using System;
using System.Collections.Generic;

public class CakeCommandBuffer
{
    private interface ICommand
    {
        void Execute(World world);
    }

    private sealed class DestroyEntityCommand : ICommand
    {
        private readonly ulong _entityId;
        public DestroyEntityCommand(ulong entityId) => _entityId = entityId;
        public void Execute(World world)
        {
            NativeCalls.Native_DestroyEntity(_entityId);
            world.RemoveEntityLocal(_entityId);
        }
    }

    private sealed class AddComponentCommand<T> : ICommand where T : CakeComponent
    {
        private readonly ulong _entityId;
        private readonly T _component;
        public AddComponentCommand(ulong entityId, T component) { _entityId = entityId; _component = component; }
        public void Execute(World world) => world.AddOrReplaceComponent(_entityId, _component);
    }

    private sealed class RemoveComponentCommand<T> : ICommand where T : CakeComponent
    {
        private readonly ulong _entityId;
        public RemoveComponentCommand(ulong entityId) => _entityId = entityId;
        public void Execute(World world) => ManagedComponentStore.Remove<T>(_entityId);
    }

    private readonly List<ICommand> _commands = new();

    public void DestroyEntity(ulong entityId)
    {
        _commands.Add(new DestroyEntityCommand(entityId));
    }

    public void AddComponent<T>(ulong entityId, T component) where T : CakeComponent
    {
        _commands.Add(new AddComponentCommand<T>(entityId, component));
    }

    public void RemoveComponent<T>(ulong entityId) where T : CakeComponent
    {
        _commands.Add(new RemoveComponentCommand<T>(entityId));
    }

    public void Apply(World world)
    {
        foreach (var cmd in _commands)
            cmd.Execute(world);
        _commands.Clear();
    }

    public bool HasCommands => _commands.Count > 0;
}
