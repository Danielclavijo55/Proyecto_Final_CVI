// FluidVisualizationShader.fx - Colores agradables y buena visibilidad de partículas
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
    // Leer velocidad en este punto
    float2 velocity = g_VelocityTexture.Sample(g_LinearSampler, PSIn.TexCoord).xy;
    
    // Calcular magnitud del flujo
    float speed = length(velocity) * 5.0; // Menor amplificación para colores más suaves
    
    // Calcular dirección como ángulo y normalizar a [0,1]
    float angle = atan2(velocity.y, velocity.x) / (3.14159265 * 2.0) + 0.5;
    
    // Definir una paleta de colores agradables (tonos azules-púrpuras)
    // Estos colores son suaves y funcionan bien incluso si convergen
    float3 color1 = float3(0.2, 0.3, 0.55); // Azul oscuro
    float3 color2 = float3(0.3, 0.4, 0.7);  // Azul medio
    float3 color3 = float3(0.5, 0.4, 0.8);  // Púrpura medio
    float3 color4 = float3(0.7, 0.4, 0.6);  // Rosa suave
    
    // Interpolar entre colores basado en el ángulo
    float3 baseColor;
    if (angle < 0.25) {
        baseColor = lerp(color1, color2, angle * 4.0);
    } else if (angle < 0.5) {
        baseColor = lerp(color2, color3, (angle - 0.25) * 4.0);
    } else if (angle < 0.75) {
        baseColor = lerp(color3, color4, (angle - 0.5) * 4.0);
    } else {
        baseColor = lerp(color4, color1, (angle - 0.75) * 4.0);
    }
    
    // Color base para velocidad cero/baja (muy suave y transparente)
    float3 zeroColor = float3(0.15, 0.15, 0.25); 
    
    // Mezclar basado en velocidad
    float3 finalColor = lerp(zeroColor, baseColor, saturate(speed));
    
    // Añadir rejilla sutil
    float2 grid = frac(PSIn.TexCoord * 15.0);
    float gridLine = (grid.x > 0.93 || grid.y > 0.93) ? 0.1 : 0.0;
    finalColor += float3(gridLine, gridLine, gridLine);
    
    // Transparencia moderada para que se vean las partículas
    // Más transparente en general, pero más opaco en zonas de alta velocidad
    float alpha = saturate(0.2 + speed * 0.2);
    
    return float4(finalColor, alpha);
}