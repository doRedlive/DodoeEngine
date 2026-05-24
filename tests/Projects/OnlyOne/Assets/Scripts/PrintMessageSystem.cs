namespace OnlyOne;

using GreenCake;

public class PrintMessageSystem : DoSystem
{
    public void Start()
    {
        Debug.Log("PrintMessageSystem Start!");
    }

    public void Update()
    {
        Debug.Log("PrintMessageSystem Update!");
        Debug.Log($"DeltaTime is {Time.DeltaTime}");

        if (World is null)
            return;

        foreach (Entity entity in World.Query<TestComponent>())
        {
            TestComponent testComp = entity.GetComponent<TestComponent>();
            Debug.Log($"Test Entity Print TestValue {testComp.TestValue}");
        }
    }

}
