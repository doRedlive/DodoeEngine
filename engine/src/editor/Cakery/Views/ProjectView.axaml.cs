using Avalonia.Controls;
using Avalonia.Input;
using Cakery.ViewModels;

namespace Cakery.Views;

public partial class ProjectView : UserControl
{
    public ProjectView() => InitializeComponent();

    private void OnFilePointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (sender is Control c && c.DataContext is FileEntry file)
        {
            if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed &&
                e.ClickCount == 2)
            {
                if (DataContext is ProjectViewModel vm)
                {
                    if (file.IsDirectory)
                        vm.NavigateIntoCommand.Execute(file);
                    else
                        vm.OpenSelectedCommand.Execute(null);
                }
            }
        }
    }
}
