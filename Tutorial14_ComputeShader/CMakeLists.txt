cmake_minimum_required (VERSION 3.10)

project(Tutorial14_ComputeShader CXX)

set(SOURCE
    src/Tutorial14_ComputeShader.cpp
    src/Tutorial14_FluidSimulation.cpp
)

set(INCLUDE
    src/Tutorial14_ComputeShader.hpp
    src/Tutorial14_FluidSimulation.hpp

)

set(SHADERS
    assets/particle.psh
    assets/particle.vsh
    assets/structures.fxh
    assets/reset_particle_lists.csh
    assets/collide_particles.csh
    assets/move_particles.csh
    assets/particles.fxh
    assets/FluidVertexShader.fx
    assets/FluidPixelShader.fx
    assets/FluidForceShader.fx
    assets/FluidVisualizationShader.fx
    assets/PaintParticle.vsh
    assets/PaintParticle.psh
    assets/RenderCanvas.psh
)

set(ASSETS)

add_sample_app("Tutorial14_ComputeShader" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
