// FluidVertexShader.fx - Para renderizar un quad de pantalla completa
struct VSInput
{
    uint VertexID : SV_VertexID;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    // Quad de pantalla completa usando VertexID
    float2 Positions[4] = {
        float2(-1.0, -1.0),
        float2(-1.0,  1.0),
        float2( 1.0, -1.0),
        float2( 1.0,  1.0)
    };
    
    float2 TexCoords[4] = {
        float2(0.0, 1.0),
        float2(0.0, 0.0),
        float2(1.0, 1.0),
        float2(1.0, 0.0)
    };
    
    PSIn.Position = float4(Positions[VSIn.VertexID], 0.0, 1.0);
    PSIn.TexCoord = TexCoords[VSIn.VertexID];
}