// Mejorar el shader de fuerzas para fluidos más dinámicos
// FluidForceShader.fx 

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
    
    // Aplicar fuerza con radio más amplio
    float extendedRadius = ForceRadius * 1.5;
    if (dist < extendedRadius)
    {
        // La fuerza disminuye con la distancia (función gaussiana)
        float factor = exp(-dist * dist / (ForceRadius * ForceRadius * 0.7));
        
        // Aumentar la intensidad de la fuerza
        float forceIntensity = 2.5; // Amplificado para mayor efecto visual
        velocity += ForceVector * factor * TimeStep * forceIntensity;
        
        // Añadir una rotación adicional para aumentar el efecto de remolino
        float2 perpendicular = float2(-delta.y, delta.x);
        perpendicular = normalize(perpendicular) * length(ForceVector) * 0.3;
        velocity += perpendicular * factor * TimeStep;
    }
    
    // Añadir un poco de fuerza aleatoria para mantener el fluido activo
    float2 noise = float2(
        sin(pixelPos.x * 50.0 + TimeStep * 2.0) * cos(pixelPos.y * 60.0 + TimeStep),
        cos(pixelPos.x * 60.0 + TimeStep) * sin(pixelPos.y * 50.0 + TimeStep * 2.0)
    ) * 0.01;
    
    velocity += noise * TimeStep;
    
    return float4(velocity, 0.0, 1.0);
}