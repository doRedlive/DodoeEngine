// do@Redlive
namespace Cakery.Models;

public class EntityInfo
{
    public ulong Id { get; set; }
    public string Name { get; set; } = "";
    public bool IsEditing { get; set; }
    public List<EntityInfo> Children { get; set; } = new();
}
