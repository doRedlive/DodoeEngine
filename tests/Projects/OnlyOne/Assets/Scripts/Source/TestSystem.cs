namespace OnlyOne;

using GreenCake;

public class TestSystem : DoSystem
{
    private Entity test = null!;
    private int frameCount;
    private bool queryLogged;

    public void Start()
    {
        test = Wolrd.CreateEntity("ScriptRuntimeTest");

        // Native side already has some default components, but the script world keeps
        // its own cache, so we mirror the components we want to read/write here.
        test.AddComponent<IDComponent>();
        test.AddComponent<TagComponent>();
        test.AddComponent<TransformComponent>();
        test.AddComponent<MeshComponent>();
        test.AddComponent<SpriteRendererComponent>();

        var id = test.GetComponent<IDComponent>();
        var tag = test.GetComponent<TagComponent>();
        var transform = test.GetComponent<TransformComponent>();
        var mesh = test.GetComponent<MeshComponent>();
        var sprite = test.GetComponent<SpriteRendererComponent>();

        tag.Tag = "ScriptRuntimeTestTag";
        transform.Position = new Vector3f(2.0f, 1.0f, 0.0f);
        transform.Rotation = new Vector3f(0.0f, 0.0f, 0.0f);
        transform.Scale = new Vector3f(1.25f, 1.25f, 1.0f);
        mesh.Value = 42;
        sprite.Color = Color.Green;
        sprite.Depth = 0.25f;
        sprite.Flip = false;
        sprite.Pivot = new Vector2f(0.5f, 0.5f);

        Debug.Log($"[TestSystem.Start] Entity={id.Name} ({id.ID}) Tag={tag.Tag} MeshValue={mesh.Value}");
    }

    public void Update()
    {
        frameCount++;

        var transform = test.GetComponent<TransformComponent>();
        var sprite = test.GetComponent<SpriteRendererComponent>();

        transform.Position = new Vector3f(
            transform.Position.X + 0.05f,
            transform.Position.Y,
            transform.Position.Z);

        if ((frameCount / 30) % 2 == 0)
            sprite.Color = Color.Red;
        else
            sprite.Color = Color.Blue;

        if (!queryLogged)
        {
            int queryCount = 0;
            bool foundTestEntity = false;

            foreach (ulong entityId in Wolrd.Query<TransformComponent, MeshComponent>())
            {
                queryCount++;
                if (entityId == test.ID)
                    foundTestEntity = true;
            }

            Debug.Log($"[TestSystem.Update] Query Count={queryCount}, FoundTest={foundTestEntity}");
            queryLogged = true;
        }

        if (frameCount % 60 == 0)
        {
            Debug.Log($"[TestSystem.Update] Frame={frameCount}, Pos=({transform.Position.X}, {transform.Position.Y}, {transform.Position.Z})");
        }
    }

    public void Finalize()
    {
        if (test != null)
        {
            Debug.Log("[TestSystem.Finalize] Destroying ScriptRuntimeTest");
            Wolrd.DestroyEntity(test);
        }
    }
}
