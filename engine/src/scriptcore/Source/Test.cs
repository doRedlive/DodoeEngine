using System;

namespace GreenCake
{
    class TestClass
    {
        public static void PrintMessage()
        {
            Console.WriteLine("Hello CSharp");
        }
    }

    struct PlayerComponent : Component
    {
        string name;
        int hp;
        int age;
    }

    class PlayerControllerSystem : doSystem
    {
        public void Update()
        {
            var transComps = Query.LookUp<PlayerComponent>();
            foreach（var transComp : transComps) 
            {
                transComp.position.x = 0.0f;
            }
        }
    }
}