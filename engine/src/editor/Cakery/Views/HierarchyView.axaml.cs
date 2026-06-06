using Avalonia.Controls;
using Avalonia.Input;
using Cakery.Models;
using Cakery.ViewModels;

namespace Cakery.Views;

public partial class HierarchyView : UserControl
{
    public HierarchyView() => InitializeComponent();

    private void OnEntityPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (sender is Control c && c.DataContext is EntityInfo entity)
        {
            if (e.GetCurrentPoint(this).Properties.IsRightButtonPressed)
            {
                // Context menu: Delete
                var menu = new ContextMenu();
                var deleteItem = new MenuItem { Header = "Delete" };
                deleteItem.Click += (_, _) =>
                {
                    if (DataContext is HierarchyViewModel vm)
                        vm.DeleteSelectedCommand.Execute(null);
                };

                var renameItem = new MenuItem { Header = "Rename" };
                renameItem.Click += (_, _) =>
                {
                    if (DataContext is HierarchyViewModel vm)
                        vm.RenameEntityCommand.Execute(entity);
                };

                menu.Items.Add(renameItem);
                menu.Items.Add(deleteItem);
                c.ContextMenu = menu;
            }
        }
    }
}
