struct PSInput
{
    float4 Position : SV_Position;
    float4 v_Color  : COLOR;
};

float4 main(PSInput input) : SV_Target
{
    return input.v_Color;
}