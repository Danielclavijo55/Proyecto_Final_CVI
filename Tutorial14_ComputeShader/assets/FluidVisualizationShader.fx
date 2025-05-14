// FluidVisualizationShader.fx - Visualizaci�n mejorada con mayor opacidad
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
    
    // Calcular magnitud del flujo - amplificar para mejor visualizaci�n
    float speed = length(velocity) * 10.0; // Amplificado por 10
    
    // Mapear la direcci�n de velocidad a un color usando HSV
    float angle = atan2(velocity.y, velocity.x) / 3.14159265359; // Normalizar a [-1, 1]
    
    // Convertir �ngulo a tono (hue)
    float hue = (angle + 1.0) * 0.5; // Normalizar a [0, 1]
    
    // Crear un color HSV - saturaci�n mayor para colores m�s vibrantes
    float3 hsv = float3(hue, 0.9, 1.0);
    
    // Convertir HSV a RGB 
    float3 rgb;
    
    // Implementaci�n de HSV a RGB
    float h = hsv.x * 6.0;
    float i = floor(h);
    float f = h - i;
    float p = hsv.z * (1.0 - hsv.y);
    float q = hsv.z * (1.0 - hsv.y * f);
    float t = hsv.z * (1.0 - hsv.y * (1.0 - f));
    
    if (i == 0.0) rgb = float3(hsv.z, t, p);
    else if (i == 1.0) rgb = float3(q, hsv.z, p);
    else if (i == 2.0) rgb = float3(p, hsv.z, t);
    else if (i == 3.0) rgb = float3(p, q, hsv.z);
    else if (i == 4.0) rgb = float3(t, p, hsv.z);
    else rgb = float3(hsv.z, p, q);
    
    // Escalar color seg�n magnitud - m�s brillante para mayor velocidad
    float brightness = saturate(speed * 1.5); // Escalar para mejor visualizaci�n
    rgb = lerp(float3(0.2, 0.2, 0.3), rgb, brightness); // Color base m�s oscuro
    
    // Efecto de l�neas de flujo - a�ade un patr�n de l�neas
    float2 scaledPos = PSIn.TexCoord * 30.0; // Escala para un patr�n m�s denso
    float2 flowOffset = velocity * TimeStep * 15.0; // Aumentado para movimiento m�s visible
    float linePattern = sin((scaledPos.x + flowOffset.x) * 3.14) * 
                       sin((scaledPos.y + flowOffset.y) * 3.14);
    linePattern = saturate(abs(linePattern) * 6.0); // M�s intenso
    
    // Mezclar el patr�n de l�neas con el color base
    rgb = lerp(rgb, float3(1.0, 1.0, 1.0), linePattern * 0.3); // Mayor intensidad
    
    // Aplicar transparencia con mayor opacidad
    float alpha = saturate(brightness * 0.6 + 0.2); // Base de 0.2 + hasta 0.6 m�s
    
    // Aumentar opacidad en zonas de mayor velocidad
    alpha = lerp(alpha, 0.8, brightness * 0.5);
    
    // Efecto de vi�eta para desvanecer en bordes
    float2 center = PSIn.TexCoord - 0.5;
    float len = length(center * 1.8); // Radio ajustado 
    float vignette = 1.0 - smoothstep(0.4, 0.75, len);
    alpha *= vignette;
    
    return float4(rgb, alpha);
}