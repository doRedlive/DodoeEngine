// do@Redlive
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using CommunityToolkit.Mvvm.Messaging;
using Cakery.Models;
using Cakery.Services;

namespace Cakery.ViewModels;

public partial class HierarchyViewModel : ObservableObject
{
    [ObservableProperty] private List<EntityInfo> _entities = new();
    [ObservableProperty] private EntityInfo? _selectedEntity;
    [ObservableProperty] private string _searchText = "";

    [RelayCommand]
    public void Refresh()
    {
        Entities.Clear();
        int count = EditorEngine.EntityGetChildCount(0);
        for (int i = 0; i < count; i++)
        {
            var id = EditorEngine.EntityGetChildAt(0, i);
            var name = EditorEngine.EntityGetName(id);
            var info = new EntityInfo { Id = id, Name = name };
            PopulateChildren(info);
            if (MatchesSearch(info)) Entities.Add(info);
        }
    }

    private void PopulateChildren(EntityInfo parent)
    {
        int count = EditorEngine.EntityGetChildCount(parent.Id);
        for (int i = 0; i < count; i++)
        {
            var id = EditorEngine.EntityGetChildAt(parent.Id, i);
            var child = new EntityInfo { Id = id, Name = EditorEngine.EntityGetName(id) };
            PopulateChildren(child);
            parent.Children.Add(child);
        }
    }

    private bool MatchesSearch(EntityInfo info)
    {
        if (string.IsNullOrWhiteSpace(SearchText)) return true;
        return info.Name.Contains(SearchText, StringComparison.OrdinalIgnoreCase);
    }

    partial void OnSearchTextChanged(string value) => Refresh();

    partial void OnSelectedEntityChanged(EntityInfo? value)
    {
        if (value != null)
        {
            EditorEngine.SelectEntity(value.Id);
            WeakReferenceMessenger.Default.Send(new EntitySelectedMessage(value));
        }
    }

    public void Deselect() => SelectedEntity = null;

    [RelayCommand]
    public void CreateEntity()
    {
        ulong id = EditorEngine.EntityCreate();
        EditorEngine.EntitySetName(id, "New Entity");
        EditorEngine.EntitySetParent(id, SelectedEntity?.Id ?? 0);
        Refresh();
    }

    [RelayCommand]
    public void DeleteSelected()
    {
        if (SelectedEntity == null) return;
        EditorEngine.EntityDestroy(SelectedEntity.Id);
        Deselect();
        WeakReferenceMessenger.Default.Send(new EntitySelectedMessage(null!));
        Refresh();
    }

    [RelayCommand]
    public void RenameEntity(EntityInfo? entity)
    {
        if (entity == null) return;
        entity.IsEditing = true;
    }

    public void CommitRename(EntityInfo entity, string newName)
    {
        entity.IsEditing = false;
        entity.Name = newName;
        EditorEngine.EntitySetName(entity.Id, newName);
    }
}
