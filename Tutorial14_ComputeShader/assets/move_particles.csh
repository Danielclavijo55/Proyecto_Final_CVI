#include "structures.fxh"
#include "particles.fxh"

cbuffer Constants
{
    GlobalConstants g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

RWStructuredBuffer<ParticleAttribs> g_Particles;

// Metal backend has a limitation that structured buffers must have
// different element types. So we use a struct to wrap the particle index.
struct HeadData
{
    int FirstParticleIdx;
};
RWStructuredBuffer<HeadData> g_ParticleListHead;

RWStructuredBuffer<int> g_ParticleLists;

// Textura de velocidad del fluido para influenciar las partículas
Texture2D<float2> g_FluidVelocityTexture;
SamplerState g_LinearSampler;

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID)
{
    uint uiGlobalThreadIdx = Gid.x * uint(THREAD_GROUP_SIZE) + GTid.x;
    if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
        return;

    int iParticleIdx = int(uiGlobalThreadIdx);

    ParticleAttribs Particle = g_Particles[iParticleIdx];
    Particle.f2Pos   = Particle.f2NewPos;
    Particle.f2Speed = Particle.f2NewSpeed;
    
    // Aplicar una fuerza adicional basada en el campo de velocidad del fluido
    // Convertir posición de partícula a coordenadas de textura [0,1]
    float2 texCoord = (Particle.f2Pos + 1.0) * 0.5;
    
    // Leer la velocidad del fluido en la posición de la partícula
    float2 fluidVelocity = g_FluidVelocityTexture.SampleLevel(g_LinearSampler, texCoord, 0).xy;
    
    // Factor de influencia del fluido sobre las partículas (ajustable)
    float fluidInfluence = 0.2; // Reducido para movimiento más calmado
    
    // Aplicar la influencia del fluido a la velocidad de la partícula
    Particle.f2Speed += fluidVelocity * fluidInfluence * g_Constants.fDeltaTime;
    
    // Actualizar posición basada en la velocidad modificada
    Particle.f2Pos += Particle.f2Speed * g_Constants.f2Scale * g_Constants.fDeltaTime;
    Particle.fTemperature -= Particle.fTemperature * min(g_Constants.fDeltaTime * 2.0, 1.0);
    
    // Aumentar temperatura si la partícula está en una zona de alta velocidad del fluido
    float fluidSpeed = length(fluidVelocity);
    Particle.fTemperature = max(Particle.fTemperature, fluidSpeed * 0.15);

    ClampParticlePosition(Particle.f2Pos, Particle.f2Speed, Particle.fSize, g_Constants.f2Scale);
    g_Particles[iParticleIdx] = Particle;

    // Bin particles
    int GridIdx = GetGridLocation(Particle.f2Pos, g_Constants.i2ParticleGridSize).z;
    int OriginalListIdx;
    InterlockedExchange(g_ParticleListHead[GridIdx].FirstParticleIdx, iParticleIdx, OriginalListIdx);
    g_ParticleLists[iParticleIdx] = OriginalListIdx;
}