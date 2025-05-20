// FluidForceShader.fx - Versión más calmada
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
    // Obtener velocidad actual
    float2 velocity = g_VelocityTexture.Sample(g_LinearSampler, PSIn.TexCoord).xy;
    
    // Calcular distancia al punto de fuerza
    float2 pixelPos = PSIn.TexCoord;
    float2 delta = pixelPos - ForcePosition;
    float dist = length(delta);
    
    // Aplicar fuerza con radio más amplio pero intensidad reducida
    float extendedRadius = ForceRadius * 1.7; // Aumentado de 1.5 a 1.7
    if (dist < extendedRadius)
    {
        // La fuerza disminuye con la distancia (función gaussiana más suave)
        float factor = exp(-dist * dist / (ForceRadius * ForceRadius * 0.9)); // Más suave
        
        // Reducir la intensidad de la fuerza para un fluido más calmado
        float forceIntensity = 1.5; // Reducido de 2.5 a 1.5
        velocity += ForceVector * factor * TimeStep * forceIntensity;
        
        // Reducir la rotación adicional para un efecto menos caótico
        float2 perpendicular = float2(-delta.y, delta.x);
        perpendicular = normalize(perpendicular) * length(ForceVector) * 0.2; // Reducido de 0.3 a 0.2
        velocity += perpendicular * factor * TimeStep;
    }
    
    // Reducir la magnitud del ruido para mantener el fluido estable
    float2 noise = float2(
        sin(pixelPos.x * 40.0 + TimeStep * 1.5) * cos(pixelPos.y * 45.0 + TimeStep * 0.8),
        cos(pixelPos.x * 45.0 + TimeStep * 0.8) * sin(pixelPos.y * 40.0 + TimeStep * 1.5)
    ) * 0.007; // Reducido de 0.01 a 0.007
    
    velocity += noise * TimeStep;
    
    // Aplicar amortiguación para un fluido más estable
    velocity *= (1.0 - TimeStep * 0.1); // Añadir pequeña amortiguación global
    
    return float4(velocity, 0.0, 1.0);
}