// do@Redlive
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using CommunityToolkit.Mvvm.Messaging;
using Cakery.Models;
using Cakery.Services;

namespace Cakery.ViewModels;

public partial class InspectorViewModel : ObservableObject, IRecipient<EntitySelectedMessage>
{
    [ObservableProperty] private EntityInfo? _entity;
    [ObservableProperty] private List<IComponentEditor> _components = new();
    [ObservableProperty] private string _entityName = "";
    [ObservableProperty] private string _searchText = "";
    [ObservableProperty] private bool _showAddComponentMenu;
    [ObservableProperty] private List<string> _availableComponents = new();

    private static readonly string[] AllComponentTypes =
    {
        "TransformComponent", "SpriteRendererComponent", "Rigidbody2dComponent",
        "BoxCollider2dComponent", "Animation2dComponent", "Camera2dComponent",
        "MeshRendererComponent", "TilemapComponent", "TileLayerComponent", "HierarchyComponent"
    };

    public InspectorViewModel() => WeakReferenceMessenger.Default.Register(this);

    public void Receive(EntitySelectedMessage msg)
    {
        Entity = msg.Entity;
        if (Entity == null) { Components.Clear(); return; }
        Refresh();
    }

    public void Refresh()
    {
        Components.Clear();
        if (Entity == null) return;
        EntityName = EditorEngine.EntityGetName(Entity.Id);
        int count = EditorEngine.EntityGetComponentCount(Entity.Id);
        for (int i = 0; i < count; i++)
        {
            var tn = EditorEngine.EntityGetComponentType(Entity.Id, i);
            var editor = CreateEditor(tn, Entity);
            if (editor != null) Components.Add(editor);
        }
    }

    private IComponentEditor? CreateEditor(string typeName, EntityInfo entity)
    {
        return typeName switch
        {
            "TransformComponent" => new TransformEditor(entity),
            "SpriteRendererComponent" => new SpriteRendererEditor(entity),
            "Rigidbody2dComponent" => new Rigidbody2DEditor(entity),
            "BoxCollider2dComponent" => new BoxCollider2DEditor(entity),
            "Camera2dComponent" => new Camera2DEditor(entity),
            _ => new GenericComponentEditor(typeName, entity)
        };
    }

    partial void OnEntityNameChanged(string value)
    {
        if (Entity != null) EditorEngine.EntitySetName(Entity.Id, value);
    }

    [RelayCommand]
    public void RemoveComponent(IComponentEditor? comp)
    {
        if (Entity == null || comp == null) return;
        EditorEngine.EntityRemoveComponent(Entity.Id, comp.TypeName);
        Refresh();
    }

    [RelayCommand]
    public void ToggleAddComponent()
    {
        ShowAddComponentMenu = !ShowAddComponentMenu;
        if (ShowAddComponentMenu && Entity != null)
        {
            AvailableComponents.Clear();
            foreach (var t in AllComponentTypes)
                if (!EditorEngine.EntityHasComponent(Entity.Id, t))
                    AvailableComponents.Add(t);
        }
    }

    [RelayCommand]
    public void AddComponent(string? typeName)
    {
        if (Entity == null || typeName == null) return;
        EditorEngine.EntityAddComponent(Entity.Id, typeName);
        ShowAddComponentMenu = false;
        Refresh();
    }
}

// ============================================================
// Component editors (mirrors C++ inspector component drawers)
// ============================================================

public interface IComponentEditor
{
    string TypeName { get; }
    bool CanRemove { get; }
}

public class GenericComponentEditor : IComponentEditor
{
    public string TypeName { get; }
    public bool CanRemove => TypeName != "TransformComponent";
    public GenericComponentEditor(string typeName, EntityInfo _) => TypeName = typeName;
}

public partial class TransformEditor : ObservableObject, IComponentEditor
{
    public string TypeName => "TransformComponent";
    public bool CanRemove => false;
    private readonly EntityInfo _entity;

    public TransformEditor(EntityInfo entity) { _entity = entity; Reload(); }

    public void Reload()
    {
        var (x, y) = EditorEngine.EntityGetPosition(_entity.Id);
        PosX = x; PosY = y;
        var (sx, sy) = EditorEngine.EntityGetScale(_entity.Id);
        ScaleX = sx; ScaleY = sy;
        RotationDeg = EditorEngine.EntityGetRotation(_entity.Id) * (180f / MathF.PI);
    }

    [ObservableProperty] private float _posX;
    [ObservableProperty] private float _posY;
    [ObservableProperty] private float _scaleX = 1;
    [ObservableProperty] private float _scaleY = 1;
    [ObservableProperty] private float _rotationDeg;

    partial void OnPosXChanged(float value) => EditorEngine.EntitySetPosition(_entity.Id, value, PosY);
    partial void OnPosYChanged(float value) => EditorEngine.EntitySetPosition(_entity.Id, PosX, value);
    partial void OnScaleXChanged(float value) => EditorEngine.EntitySetScale(_entity.Id, value, ScaleY);
    partial void OnScaleYChanged(float value) => EditorEngine.EntitySetScale(_entity.Id, ScaleX, value);
    partial void OnRotationDegChanged(float value) => EditorEngine.EntitySetRotation(_entity.Id, value * (MathF.PI / 180f));
}

public partial class SpriteRendererEditor : ObservableObject, IComponentEditor
{
    public string TypeName => "SpriteRendererComponent";
    public bool CanRemove => true;
    private readonly EntityInfo _entity;

    public SpriteRendererEditor(EntityInfo entity) { _entity = entity; }

    [ObservableProperty] private float _colorR = 1;
    [ObservableProperty] private float _colorG = 1;
    [ObservableProperty] private float _colorB = 1;
    [ObservableProperty] private float _colorA = 1;
    [ObservableProperty] private float _pivotX;
    [ObservableProperty] private float _pivotY;
    [ObservableProperty] private float _depth;
    [ObservableProperty] private bool _flip;
    [ObservableProperty] private string _texturePath = "";
}

public partial class Rigidbody2DEditor : ObservableObject, IComponentEditor
{
    public string TypeName => "Rigidbody2dComponent";
    public bool CanRemove => true;
    public Rigidbody2DEditor(EntityInfo _) { }

    [ObservableProperty] private float _mass = 1;
    [ObservableProperty] private float _gravityScale = 1;
    [ObservableProperty] private float _linearDamping;
    [ObservableProperty] private float _angularDamping;
    [ObservableProperty] private bool _fixedRotation;
    [ObservableProperty] private int _bodyType; // 0=Dynamic, 1=Kinematic, 2=Static
}

public partial class BoxCollider2DEditor : ObservableObject, IComponentEditor
{
    public string TypeName => "BoxCollider2dComponent";
    public bool CanRemove => true;
    public BoxCollider2DEditor(EntityInfo _) { }

    [ObservableProperty] private float _offsetX;
    [ObservableProperty] private float _offsetY;
    [ObservableProperty] private float _sizeX = 1;
    [ObservableProperty] private float _sizeY = 1;
    [ObservableProperty] private float _density = 1;
    [ObservableProperty] private float _friction = 0.4f;
    [ObservableProperty] private float _restitution;
    [ObservableProperty] private bool _isTrigger;
}

public partial class Camera2DEditor : ObservableObject, IComponentEditor
{
    public string TypeName => "Camera2dComponent";
    public bool CanRemove => true;
    public Camera2DEditor(EntityInfo _) { }

    [ObservableProperty] private float _orthoSize = 5;
    [ObservableProperty] private float _nearClip = 0.1f;
    [ObservableProperty] private float _farClip = 100;
    [ObservableProperty] private bool _isMainCamera = true;
}
