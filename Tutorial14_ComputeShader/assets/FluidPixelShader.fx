// FluidPixelShader.fx - Shader de advección simplificado
Texture2D g_VelocityTexture;
SamplerState g_LinearSampler;

cbuffer cbFluidConstants
{
    float TimeStep;
    float Viscosity;
    float GridScale;
    float Padding0;
    
    float2 InverseGridSize;
    float2 ForcePosition;
    
    float2 ForceVector;
    float ForceRadius;
    float Padding1;
}

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 main(PSInput PSIn) : SV_TARGET
{
    // Advección: trazar el campo de velocidad hacia atrás en el tiempo
    float2 pos = PSIn.TexCoord;
    float2 velocity = g_VelocityTexture.Sample(g_LinearSampler, pos).xy;
    
    // Trazar hacia atrás para encontrar la velocidad anterior
    float2 prevPos = pos - velocity * TimeStep * InverseGridSize;
    float2 prevVelocity = g_VelocityTexture.Sample(g_LinearSampler, prevPos).xy;
    
    // Aplicar difusión basada en viscosidad
    float2 result = lerp(velocity, prevVelocity, TimeStep * Viscosity);
    
    // Aplicar un pequeño factor de amortiguación 
    result *= (1.0 - TimeStep * 0.1);
    
    return float4(result, 0.0, 1.0);
}