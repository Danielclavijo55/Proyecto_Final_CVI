// PaintParticle.vsh - Vertex shader para pintar part�culas
#include "structures.fxh"

StructuredBuffer<ParticleAttribs> g_Particles;

struct VSInput
{
    uint VertID : SV_VertexID;
    uint InstID : SV_InstanceID;
};

struct PSInput 
{ 
    float4 Pos        : SV_POSITION; 
    float2 uv         : TEXCOORD0;
    float2 worldPos   : TEXCOORD1;     // Usaremos esto como "semilla" de color
    float  temp       : TEXCOORD2;
    float  speed      : TEXCOORD3;
};

void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    // Configurar vertices del quad
    float4 pos_uv[4];
    pos_uv[0] = float4(-1.0,+1.0, 0.0,0.0);
    pos_uv[1] = float4(-1.0,-1.0, 0.0,1.0);
    pos_uv[2] = float4(+1.0,+1.0, 1.0,0.0);
    pos_uv[3] = float4(+1.0,-1.0, 1.0,1.0);

    ParticleAttribs Attribs = g_Particles[VSIn.InstID];

    // Calcular el tama�o del trazo basado en la velocidad (trazos m�s grandes)
    float particleSpeed = length(Attribs.f2Speed);
    float brushSize = Attribs.fSize * (1.2 + particleSpeed * 0.5); // Brush m�s grande
    
    // Crear el quad para la "pincelada"
    float2 pos = pos_uv[VSIn.VertID].xy;
    pos = pos * brushSize + Attribs.f2Pos;
    
    PSIn.Pos = float4(pos, 0.0, 1.0);
    PSIn.uv = pos_uv[VSIn.VertID].zw;
    
    // CLAVE: Usar una combinaci�n de posici�n inicial + ID de instancia como semilla de color
    // Esto hace que cada part�cula tenga un color "personal" consistente
    float2 colorSeed;
    colorSeed.x = frac(sin(float(VSIn.InstID) * 12.9898) * 43758.5453); // Pseudo-random basado en ID
    colorSeed.y = frac(cos(float(VSIn.InstID) * 78.233) * 43758.5453);  // Otra componente pseudo-random
    
    // Mezclar con posici�n inicial para m�s variaci�n
    colorSeed += (Attribs.f2Pos + 1.0) * 0.1; // Peque�a contribuci�n de posici�n
    
    PSIn.worldPos = colorSeed; // Esto ser� la "semilla personal" de cada part�cula
    PSIn.temp = Attribs.fTemperature;
    PSIn.speed = particleSpeed;
}