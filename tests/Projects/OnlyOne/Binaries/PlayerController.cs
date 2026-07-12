namespace OnlyOne;

using GreenCake;

public class PlayerController : CakeBehaviour
{
    private GameObject _player;

    private void Awake()
    {
        _player = GameObject.Create("Player");

    }
}
