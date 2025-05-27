/*
 *  Copyright 2019-2024 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include <random>
#include "Tutorial14_FluidSimulation.hpp"
#include "Tutorial14_ComputeShader.hpp"
#include "BasicMath.hpp"
#include "MapHelper.hpp"
#include "imgui.h"
#include "ShaderMacroHelper.hpp"
#include "ColorConversion.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial14_ComputeShader();
}

namespace
{

struct ParticleAttribs
{
    float2 f2Pos;
    float2 f2NewPos;

    float2 f2Speed;
    float2 f2NewSpeed;

    float fSize          = 0;
    float fTemperature   = 0;
    int   iNumCollisions = 0;
    float fPadding0      = 0;
};

} // namespace

void Tutorial14_ComputeShader::CreateRenderParticlePSO()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    PSOCreateInfo.PSODesc.Name = "Render particles PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // Disable back face culling
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    // Disable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    // clang-format on

    auto& BlendDesc = PSOCreateInfo.GraphicsPipeline.BlendDesc;

    BlendDesc.RenderTargets[0].BlendEnable    = True;
    BlendDesc.RenderTargets[0].SrcBlend       = BLEND_FACTOR_SRC_ALPHA;
    BlendDesc.RenderTargets[0].DestBlend      = BLEND_FACTOR_INV_SRC_ALPHA;
    BlendDesc.RenderTargets[0].BlendOp        = BLEND_OPERATION_ADD;
    BlendDesc.RenderTargets[0].SrcBlendAlpha  = BLEND_FACTOR_ONE;
    BlendDesc.RenderTargets[0].DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    BlendDesc.RenderTargets[0].BlendOpAlpha   = BLEND_OPERATION_ADD;

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // Presentation engine always expects input in gamma space. Normally, pixel shader output is
    // converted from linear to gamma space by the GPU. However, some platforms (e.g. Android in GLES mode,
    // or Emscripten in WebGL mode) do not support gamma-correction. In this case the application
    // has to do the conversion manually.
    ShaderMacro Macros[] = {{"CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros      = {Macros, _countof(Macros)};

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create particle vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Particle VS";
        ShaderCI.FilePath        = "particle.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    // Create particle pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Particle PS";
        ShaderCI.FilePath        = "particle.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // clang-format off
    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_VERTEX, "g_Particles", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    // clang-format on
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pRenderParticlePSO);
    m_pRenderParticlePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_Constants);
}

void Tutorial14_ComputeShader::CreateUpdateParticlePSO()
{
    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    ShaderMacroHelper Macros;
    Macros.AddShaderMacro("THREAD_GROUP_SIZE", m_ThreadGroupSize);

    RefCntAutoPtr<IShader> pResetParticleListsCS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Reset particle lists CS";
        ShaderCI.FilePath        = "reset_particle_lists.csh";
        ShaderCI.Macros          = Macros;
        m_pDevice->CreateShader(ShaderCI, &pResetParticleListsCS);
    }

    RefCntAutoPtr<IShader> pMoveParticlesCS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Move particles CS";
        ShaderCI.FilePath        = "move_particles.csh";
        ShaderCI.Macros          = Macros;
        m_pDevice->CreateShader(ShaderCI, &pMoveParticlesCS);
    }

    RefCntAutoPtr<IShader> pCollideParticlesCS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Collide particles CS";
        ShaderCI.FilePath        = "collide_particles.csh";
        ShaderCI.Macros          = Macros;
        m_pDevice->CreateShader(ShaderCI, &pCollideParticlesCS);
    }

    RefCntAutoPtr<IShader> pUpdatedSpeedCS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_COMPUTE;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Update particle speed CS";
        ShaderCI.FilePath        = "collide_particles.csh";
        Macros.AddShaderMacro("UPDATE_SPEED", 1);
        ShaderCI.Macros = Macros;
        m_pDevice->CreateShader(ShaderCI, &pUpdatedSpeedCS);
    }

    ComputePipelineStateCreateInfo PSOCreateInfo;
    PipelineStateDesc&             PSODesc = PSOCreateInfo.PSODesc;

    // This is a compute pipeline
    PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    // clang-format off
    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_COMPUTE, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
    };
    // clang-format on
    PSODesc.ResourceLayout.Variables    = Vars;
    PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    PSODesc.Name      = "Reset particle lists PSO";
    PSOCreateInfo.pCS = pResetParticleListsCS;
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_pResetParticleListsPSO);
    m_pResetParticleListsPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants")->Set(m_Constants);

    PSODesc.Name      = "Move particles PSO";
    PSOCreateInfo.pCS = pMoveParticlesCS;
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_pMoveParticlesPSO);
    m_pMoveParticlesPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants")->Set(m_Constants);

    PSODesc.Name      = "Collidse particles PSO";
    PSOCreateInfo.pCS = pCollideParticlesCS;
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_pCollideParticlesPSO);
    m_pCollideParticlesPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants")->Set(m_Constants);

    PSODesc.Name      = "Update particle speed PSO";
    PSOCreateInfo.pCS = pUpdatedSpeedCS;
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_pUpdateParticleSpeedPSO);
    m_pUpdateParticleSpeedPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "Constants")->Set(m_Constants);
}

void Tutorial14_ComputeShader::CreateParticleBuffers()
{
    m_pParticleAttribsBuffer.Release();
    m_pParticleListHeadsBuffer.Release();
    m_pParticleListsBuffer.Release();

    BufferDesc BuffDesc;
    BuffDesc.Name              = "Particle attribs buffer";
    BuffDesc.Usage             = USAGE_DEFAULT;
    BuffDesc.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = sizeof(ParticleAttribs);
    BuffDesc.Size              = sizeof(ParticleAttribs) * m_NumParticles;

    std::vector<ParticleAttribs> ParticleData(m_NumParticles);

    std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
                      // to generate consistent distribution.

    std::uniform_real_distribution<float> pos_distr(-1.f, +1.f);
    std::uniform_real_distribution<float> size_distr(0.5f, 1.f);

    constexpr float fMaxParticleSize = 0.05f;
    float           fSize            = 0.7f / std::sqrt(static_cast<float>(m_NumParticles));
    fSize                            = std::min(fMaxParticleSize, fSize);
    for (auto& particle : ParticleData)
    {
        particle.f2NewPos.x   = pos_distr(gen);
        particle.f2NewPos.y   = pos_distr(gen);
        particle.f2NewSpeed.x = pos_distr(gen) * fSize * 5.f;
        particle.f2NewSpeed.y = pos_distr(gen) * fSize * 5.f;
        particle.fSize        = fSize * size_distr(gen);
    }

    BufferData VBData;
    VBData.pData    = ParticleData.data();
    VBData.DataSize = sizeof(ParticleAttribs) * static_cast<Uint32>(ParticleData.size());
    m_pDevice->CreateBuffer(BuffDesc, &VBData, &m_pParticleAttribsBuffer);
    IBufferView* pParticleAttribsBufferSRV = m_pParticleAttribsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
    IBufferView* pParticleAttribsBufferUAV = m_pParticleAttribsBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);

    BuffDesc.ElementByteStride = sizeof(int);
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.Size              = Uint64{BuffDesc.ElementByteStride} * static_cast<Uint64>(m_NumParticles);
    BuffDesc.BindFlags         = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pParticleListHeadsBuffer);
    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pParticleListsBuffer);
    IBufferView* pParticleListHeadsBufferUAV = m_pParticleListHeadsBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
    IBufferView* pParticleListsBufferUAV     = m_pParticleListsBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS);
    IBufferView* pParticleListHeadsBufferSRV = m_pParticleListHeadsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
    IBufferView* pParticleListsBufferSRV     = m_pParticleListsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);

    m_pResetParticleListsSRB.Release();
    m_pResetParticleListsPSO->CreateShaderResourceBinding(&m_pResetParticleListsSRB, true);
    m_pResetParticleListsSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ParticleListHead")->Set(pParticleListHeadsBufferUAV);

    m_pRenderParticleSRB.Release();
    m_pRenderParticlePSO->CreateShaderResourceBinding(&m_pRenderParticleSRB, true);
    m_pRenderParticleSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_Particles")->Set(pParticleAttribsBufferSRV);

    m_pMoveParticlesSRB.Release();
    m_pMoveParticlesPSO->CreateShaderResourceBinding(&m_pMoveParticlesSRB, true);
    m_pMoveParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Particles")->Set(pParticleAttribsBufferUAV);
    m_pMoveParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ParticleListHead")->Set(pParticleListHeadsBufferUAV);
    m_pMoveParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ParticleLists")->Set(pParticleListsBufferUAV);

    m_pCollideParticlesSRB.Release();
    m_pCollideParticlesPSO->CreateShaderResourceBinding(&m_pCollideParticlesSRB, true);
    m_pCollideParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Particles")->Set(pParticleAttribsBufferUAV);
    m_pCollideParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ParticleListHead")->Set(pParticleListHeadsBufferSRV);
    m_pCollideParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ParticleLists")->Set(pParticleListsBufferSRV);

    RecreatePaintSRB();
}

void Tutorial14_ComputeShader::CreateConsantBuffer()
{
    BufferDesc BuffDesc;
    BuffDesc.Name           = "Constants buffer";
    BuffDesc.Usage          = USAGE_DYNAMIC;
    BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
    BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    BuffDesc.Size           = sizeof(float4) * 2;
    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_Constants);
}

void Tutorial14_ComputeShader::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::InputInt("Num Particles", &m_NumParticles, 100, 1000, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            m_NumParticles = std::min(std::max(m_NumParticles, 100), 100000);
            CreateParticleBuffers();
        }
        ImGui::SliderFloat("Simulation Speed", &m_fSimulationSpeed, 0.1f, 5.f);
        ImGui::SliderFloat("Fluid Viscosity", &m_fViscosity, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::Text("Visualization Mode:");

        // Radio buttons para seleccionar modo de visualización
        if (ImGui::RadioButton("Fluid Visualization", m_VisualizationMode == VisualizationMode::FLUID_VISUALIZATION))
        {
            m_VisualizationMode = VisualizationMode::FLUID_VISUALIZATION;
        }

        if (ImGui::RadioButton("Paint Canvas", m_VisualizationMode == VisualizationMode::PAINT_CANVAS))
        {
            m_VisualizationMode = VisualizationMode::PAINT_CANVAS;
        }

        // Solo mostrar la opción de fluido si estamos en modo fluido
        if (m_VisualizationMode == VisualizationMode::FLUID_VISUALIZATION)
        {
            ImGui::Checkbox("Show Fluid Visualization", &m_bShowFluidVisualization);
        }
        else if (m_VisualizationMode == VisualizationMode::PAINT_CANVAS)
        {
            // Controles del Paint Canvas
            if (ImGui::Button("Clear Canvas"))
            {
                ClearCanvas();
            }

            ImGui::SameLine();
            ImGui::Text("| Tip: Try different particle counts!");
        }
    }
    ImGui::End();
}

void Tutorial14_ComputeShader::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);

    Attribs.EngineCI.Features.ComputeShaders = DEVICE_FEATURE_STATE_ENABLED;
}

void Tutorial14_ComputeShader::CreatePaintSystem()
{
    try
    {
        CreateCanvasTexture();
        CreateColorPalette();
        CreatePaintPipelines();
        LOG_INFO_MESSAGE("Paint system created successfully");
    }
    catch (const std::exception& e)
    {
        LOG_ERROR_MESSAGE("Failed to create paint system: %s", e.what());
    }
}

void Tutorial14_ComputeShader::CreateCanvasTexture()
{
    // Crear textura persistente para el canvas
    TextureDesc CanvasTexDesc;
    CanvasTexDesc.Name              = "Paint Canvas";
    CanvasTexDesc.Type              = RESOURCE_DIM_TEX_2D;
    CanvasTexDesc.Width             = m_pSwapChain->GetDesc().Width;
    CanvasTexDesc.Height            = m_pSwapChain->GetDesc().Height;
    CanvasTexDesc.Format            = TEX_FORMAT_RGBA8_UNORM;
    CanvasTexDesc.BindFlags         = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    CanvasTexDesc.ClearValue.Format = TEX_FORMAT_RGBA8_UNORM;
    // Canvas inicialmente transparente
    CanvasTexDesc.ClearValue.Color[0] = 0.0f;
    CanvasTexDesc.ClearValue.Color[1] = 0.0f;
    CanvasTexDesc.ClearValue.Color[2] = 0.0f;
    CanvasTexDesc.ClearValue.Color[3] = 0.0f;

    m_pDevice->CreateTexture(CanvasTexDesc, nullptr, &m_pCanvasTexture);

    if (m_pCanvasTexture)
    {
        m_pCanvasRTV = m_pCanvasTexture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        m_pCanvasSRV = m_pCanvasTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        // Limpiar el canvas inicialmente
        float4 ClearColor = {0.0f, 0.0f, 0.0f, 0.0f};
        m_pImmediateContext->ClearRenderTarget(m_pCanvasRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    else
    {
        LOG_ERROR_MESSAGE("Failed to create canvas texture");
    }
}

void Tutorial14_ComputeShader::CreateColorPalette()
{
    // Crear una paleta de colores vivos y primarios
    const int            PALETTE_SIZE = 256;
    std::vector<uint8_t> paletteData(PALETTE_SIZE * PALETTE_SIZE * 4);

    // Definir colores primarios vivos
    struct Color
    {
        uint8_t r, g, b, a;
    };
    Color primaryColors[] = {
        {255, 0, 0, 255},    // Rojo brillante
        {255, 165, 0, 255},  // Naranja vibrante
        {255, 255, 0, 255},  // Amarillo intenso
        {0, 255, 0, 255},    // Verde puro
        {0, 255, 255, 255},  // Cian brillante
        {0, 0, 255, 255},    // Azul puro
        {128, 0, 128, 255},  // Púrpura
        {255, 20, 147, 255}, // Rosa vibrante
    };

    int numColors = sizeof(primaryColors) / sizeof(Color);

    // Llenar la paleta con gradientes y variaciones
    for (int y = 0; y < PALETTE_SIZE; y++)
    {
        for (int x = 0; x < PALETTE_SIZE; x++)
        {
            // Usar coordenadas para crear patrones de colores
            float fx = static_cast<float>(x) / PALETTE_SIZE;
            float fy = static_cast<float>(y) / PALETTE_SIZE;

            // Seleccionar color base basado en la posición
            int   colorIndex = static_cast<int>((fx + fy * 0.7f) * numColors) % numColors;
            Color baseColor  = primaryColors[colorIndex];

            // Añadir algo de variación para crear transiciones suaves
            float variation = sin(fx * 8.0f) * cos(fy * 6.0f) * 0.3f + 0.7f;

            int index              = (y * PALETTE_SIZE + x) * 4;
            paletteData[index + 0] = static_cast<uint8_t>(baseColor.r * variation);
            paletteData[index + 1] = static_cast<uint8_t>(baseColor.g * variation);
            paletteData[index + 2] = static_cast<uint8_t>(baseColor.b * variation);
            paletteData[index + 3] = baseColor.a;
        }
    }

    // Crear la textura
    TextureDesc PaletteTexDesc;
    PaletteTexDesc.Name      = "Color Palette";
    PaletteTexDesc.Type      = RESOURCE_DIM_TEX_2D;
    PaletteTexDesc.Width     = PALETTE_SIZE;
    PaletteTexDesc.Height    = PALETTE_SIZE;
    PaletteTexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
    PaletteTexDesc.BindFlags = BIND_SHADER_RESOURCE;

    TextureSubResData InitData;
    InitData.pData  = paletteData.data();
    InitData.Stride = PALETTE_SIZE * 4;

    TextureData TexData;
    TexData.pSubResources   = &InitData;
    TexData.NumSubresources = 1;

    m_pDevice->CreateTexture(PaletteTexDesc, &TexData, &m_pColorPaletteTexture);

    if (m_pColorPaletteTexture)
    {
        m_pColorPaletteSRV = m_pColorPaletteTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
    else
    {
        LOG_ERROR_MESSAGE("Failed to create color palette texture");
    }
}

void Tutorial14_ComputeShader::CreatePaintPipelines()
{
    // Crear factory de shaders
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.pShaderSourceStreamFactory      = pShaderSourceFactory;

    // === Pipeline para pintar partículas ===

    // Vertex shader para pintar partículas
    RefCntAutoPtr<IShader> pPaintParticleVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Paint Particle VS";
        ShaderCI.FilePath        = "PaintParticle.vsh";
        m_pDevice->CreateShader(ShaderCI, &pPaintParticleVS);
    }

    // Pixel shader para pintar partículas
    RefCntAutoPtr<IShader> pPaintParticlePS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Paint Particle PS";
        ShaderCI.FilePath        = "PaintParticle.psh";
        m_pDevice->CreateShader(ShaderCI, &pPaintParticlePS);
    }

    // Crear PSO para pintar partículas
    GraphicsPipelineStateCreateInfo PaintPSOCreateInfo;
    PaintPSOCreateInfo.PSODesc.Name         = "Paint Particle PSO";
    PaintPSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    PaintPSOCreateInfo.pVS                  = pPaintParticleVS;
    PaintPSOCreateInfo.pPS                  = pPaintParticlePS;

    auto& PaintGraphicsPipeline                        = PaintPSOCreateInfo.GraphicsPipeline;
    PaintGraphicsPipeline.NumRenderTargets             = 1;
    PaintGraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_RGBA8_UNORM;
    PaintGraphicsPipeline.DSVFormat                    = TEX_FORMAT_UNKNOWN;
    PaintGraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PaintGraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    PaintGraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    // Configurar blending para acumulación de pintura
    auto& PaintBlendDesc                           = PaintGraphicsPipeline.BlendDesc;
    PaintBlendDesc.RenderTargets[0].BlendEnable    = True;
    PaintBlendDesc.RenderTargets[0].SrcBlend       = BLEND_FACTOR_SRC_ALPHA;
    PaintBlendDesc.RenderTargets[0].DestBlend      = BLEND_FACTOR_INV_SRC_ALPHA;
    PaintBlendDesc.RenderTargets[0].BlendOp        = BLEND_OPERATION_ADD;
    PaintBlendDesc.RenderTargets[0].SrcBlendAlpha  = BLEND_FACTOR_ONE;
    PaintBlendDesc.RenderTargets[0].DestBlendAlpha = BLEND_FACTOR_ONE;
    PaintBlendDesc.RenderTargets[0].BlendOpAlpha   = BLEND_OPERATION_ADD;

    PaintPSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    m_pDevice->CreateGraphicsPipelineState(PaintPSOCreateInfo, &m_pPaintParticlePSO);

    // === Pipeline para renderizar canvas ===

    // Pixel shader para renderizar canvas
    RefCntAutoPtr<IShader> pRenderCanvasPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Render Canvas PS";
        ShaderCI.FilePath        = "RenderCanvas.psh";
        m_pDevice->CreateShader(ShaderCI, &pRenderCanvasPS);
    }

    // Usar el vertex shader del fluido para el fullscreen quad
    RefCntAutoPtr<IShader> pFullScreenVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Full Screen VS";
        ShaderCI.FilePath        = "FluidVertexShader.fx";
        m_pDevice->CreateShader(ShaderCI, &pFullScreenVS);
    }

    // Crear PSO para renderizar canvas
    GraphicsPipelineStateCreateInfo CanvasPSOCreateInfo;
    CanvasPSOCreateInfo.PSODesc.Name         = "Render Canvas PSO";
    CanvasPSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    CanvasPSOCreateInfo.pVS                  = pFullScreenVS;
    CanvasPSOCreateInfo.pPS                  = pRenderCanvasPS;

    auto& CanvasGraphicsPipeline                        = CanvasPSOCreateInfo.GraphicsPipeline;
    CanvasGraphicsPipeline.NumRenderTargets             = 1;
    CanvasGraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    CanvasGraphicsPipeline.DSVFormat                    = TEX_FORMAT_UNKNOWN;
    CanvasGraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    CanvasGraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    CanvasGraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    // Sin blending para el canvas final
    CanvasGraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = False;

    CanvasPSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    m_pDevice->CreateGraphicsPipelineState(CanvasPSOCreateInfo, &m_pRenderCanvasPSO);

    // === Crear buffer de constantes para paint ===
    BufferDesc PaintBuffDesc;
    PaintBuffDesc.Name           = "Paint constants buffer";
    PaintBuffDesc.Usage          = USAGE_DYNAMIC;
    PaintBuffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
    PaintBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    PaintBuffDesc.Size           = sizeof(float4); // Time, Chaos, ScreenSize.x, ScreenSize.y
    m_pDevice->CreateBuffer(PaintBuffDesc, nullptr, &m_pPaintConstants);

    // === Crear SRBs ===
    if (m_pPaintParticlePSO)
    {
        m_pPaintParticlePSO->CreateShaderResourceBinding(&m_pPaintParticleSRB, true);

        // Configurar SRB para pintar partículas
        if (m_pPaintParticleSRB)
        {
            // Vincular buffer de partículas
            IBufferView* pParticleAttribsBufferSRV = m_pParticleAttribsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
            auto*        pParticlesVar             = m_pPaintParticleSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_Particles");
            if (pParticlesVar)
            {
                pParticlesVar->Set(pParticleAttribsBufferSRV);
            }

            // Vincular paleta de colores
            if (m_pColorPaletteSRV)
            {
                auto* pPaletteVar = m_pPaintParticleSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ColorPalette");
                if (pPaletteVar)
                {
                    pPaletteVar->Set(m_pColorPaletteSRV);
                }
            }

            // Crear y vincular sampler para la paleta
            SamplerDesc SamDesc;
            SamDesc.MinFilter = FILTER_TYPE_LINEAR;
            SamDesc.MagFilter = FILTER_TYPE_LINEAR;
            SamDesc.MipFilter = FILTER_TYPE_LINEAR;
            SamDesc.AddressU  = TEXTURE_ADDRESS_WRAP; // WRAP para repetir la paleta
            SamDesc.AddressV  = TEXTURE_ADDRESS_WRAP;

            RefCntAutoPtr<ISampler> pPaletteSampler;
            m_pDevice->CreateSampler(SamDesc, &pPaletteSampler);

            auto* pSamplerVar = m_pPaintParticleSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_LinearSampler");
            if (pSamplerVar)
            {
                pSamplerVar->Set(pPaletteSampler);
            }

            // Vincular buffer de constantes
            auto* pConstantsVar = m_pPaintParticleSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbPaintConstants");
            if (pConstantsVar)
            {
                pConstantsVar->Set(m_pPaintConstants);
            }
        }
    }

    if (m_pRenderCanvasPSO)
    {
        m_pRenderCanvasPSO->CreateShaderResourceBinding(&m_pRenderCanvasSRB, true);

        // Vincular textura del canvas
        if (m_pRenderCanvasSRB && m_pCanvasSRV)
        {
            auto* pCanvasVar = m_pRenderCanvasSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_CanvasTexture");
            if (pCanvasVar)
            {
                pCanvasVar->Set(m_pCanvasSRV);
            }

            // Crear y vincular sampler
            SamplerDesc SamDesc;
            SamDesc.MinFilter = FILTER_TYPE_LINEAR;
            SamDesc.MagFilter = FILTER_TYPE_LINEAR;
            SamDesc.MipFilter = FILTER_TYPE_LINEAR;
            SamDesc.AddressU  = TEXTURE_ADDRESS_CLAMP;
            SamDesc.AddressV  = TEXTURE_ADDRESS_CLAMP;

            RefCntAutoPtr<ISampler> pSampler;
            m_pDevice->CreateSampler(SamDesc, &pSampler);

            auto* pSamplerVar = m_pRenderCanvasSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_LinearSampler");
            if (pSamplerVar)
            {
                pSamplerVar->Set(pSampler);
            }
        }
    }

    LOG_INFO_MESSAGE("Paint pipelines created successfully");
}

void Tutorial14_ComputeShader::RenderPaintCanvas()
{
    if (!m_pRenderCanvasPSO || !m_pRenderCanvasSRB)
        return;

    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();

    // Configurar render target
    m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Configurar pipeline
    m_pImmediateContext->SetPipelineState(m_pRenderCanvasPSO);
    m_pImmediateContext->CommitShaderResources(m_pRenderCanvasSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Configurar viewport
    Viewport VP;
    VP.Width    = static_cast<float>(m_pSwapChain->GetDesc().Width);
    VP.Height   = static_cast<float>(m_pSwapChain->GetDesc().Height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    m_pImmediateContext->SetViewports(1, &VP, 0, 0);

    // Dibujar fullscreen quad
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    m_pImmediateContext->Draw(drawAttrs);
}

void Tutorial14_ComputeShader::PaintParticlesToCanvas()
{
    if (!m_pPaintParticlePSO || !m_pPaintParticleSRB || !m_pCanvasRTV)
        return;

    // Actualizar buffer de constantes de paint
    if (m_pPaintConstants)
    {
        struct PaintConstants
        {
            float  Time;
            float  Chaos;
            float2 ScreenSize;
        };

        MapHelper<PaintConstants> Constants(m_pImmediateContext, m_pPaintConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        Constants->Time       = m_fAccumulatedTime; // Pasar tiempo acumulado
        Constants->Chaos      = 1.0f;
        Constants->ScreenSize = float2(
            static_cast<float>(m_pSwapChain->GetDesc().Width),
            static_cast<float>(m_pSwapChain->GetDesc().Height));
    }

    // Configurar render target al canvas
    ITextureView* pCanvasRTVs[] = {m_pCanvasRTV};
    m_pImmediateContext->SetRenderTargets(1, pCanvasRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Configurar viewport para el canvas
    Viewport VP;
    VP.Width    = static_cast<float>(m_pSwapChain->GetDesc().Width);
    VP.Height   = static_cast<float>(m_pSwapChain->GetDesc().Height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    m_pImmediateContext->SetViewports(1, &VP, 0, 0);

    // Configurar pipeline de pintura
    m_pImmediateContext->SetPipelineState(m_pPaintParticlePSO);
    m_pImmediateContext->CommitShaderResources(m_pPaintParticleSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Dibujar las partículas como instancias
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices  = 4; // Quad
    drawAttrs.NumInstances = static_cast<Uint32>(m_NumParticles);
    m_pImmediateContext->Draw(drawAttrs);
}

void Tutorial14_ComputeShader::ClearCanvas()
{
    if (m_pCanvasRTV)
    {
        // Limpiar el canvas a transparente
        float4 ClearColor = {0.0f, 0.0f, 0.0f, 0.0f};
        m_pImmediateContext->ClearRenderTarget(m_pCanvasRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        LOG_INFO_MESSAGE("Canvas cleared");
    }
}

void Tutorial14_ComputeShader::RecreatePaintSRB()
{
    if (!m_pPaintParticlePSO)
        return;

    // Recrear el SRB desde cero
    m_pPaintParticleSRB.Release();
    m_pPaintParticlePSO->CreateShaderResourceBinding(&m_pPaintParticleSRB, true);

    // Configurar todas las variables del SRB nuevamente
    if (m_pPaintParticleSRB)
    {
        // Vincular buffer de partículas (recién creado)
        IBufferView* pParticleAttribsBufferSRV = m_pParticleAttribsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        auto*        pParticlesVar             = m_pPaintParticleSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_Particles");
        if (pParticlesVar)
        {
            pParticlesVar->Set(pParticleAttribsBufferSRV);
        }

        // Vincular paleta de colores
        if (m_pColorPaletteSRV)
        {
            auto* pPaletteVar = m_pPaintParticleSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ColorPalette");
            if (pPaletteVar)
            {
                pPaletteVar->Set(m_pColorPaletteSRV);
            }
        }

        // Crear y vincular sampler para la paleta
        SamplerDesc SamDesc;
        SamDesc.MinFilter = FILTER_TYPE_LINEAR;
        SamDesc.MagFilter = FILTER_TYPE_LINEAR;
        SamDesc.MipFilter = FILTER_TYPE_LINEAR;
        SamDesc.AddressU  = TEXTURE_ADDRESS_WRAP;
        SamDesc.AddressV  = TEXTURE_ADDRESS_WRAP;

        RefCntAutoPtr<ISampler> pPaletteSampler;
        m_pDevice->CreateSampler(SamDesc, &pPaletteSampler);

        auto* pSamplerVar = m_pPaintParticleSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_LinearSampler");
        if (pSamplerVar)
        {
            pSamplerVar->Set(pPaletteSampler);
        }

        // Vincular buffer de constantes
        auto* pConstantsVar = m_pPaintParticleSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbPaintConstants");
        if (pConstantsVar)
        {
            pConstantsVar->Set(m_pPaintConstants);
        }
    }
}

void Tutorial14_ComputeShader::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    // Inicializar sistema de partículas
    CreateConsantBuffer();
    CreateRenderParticlePSO();
    CreateUpdateParticlePSO();
    CreateParticleBuffers();

    // Crear sistema de fluidos independiente - pasar el SwapChain al constructor
    try
    {
        m_pFluidSim = std::make_unique<Tutorial14_FluidSimulation>(
            m_pDevice, m_pImmediateContext, m_pEngineFactory, m_pSwapChain);
        LOG_INFO_MESSAGE("Tutorial14_FluidSimulation created successfully");
    }
    catch (const std::exception& e)
    {
        LOG_ERROR_MESSAGE("Failed to create fluid simulation: %s", e.what());
        // Continuar sin fluidos si hay error
    }
    CreatePaintSystem();
}

// Render a frame
void Tutorial14_ComputeShader::Render()
{
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();

    // Clear the back buffer
    float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
    if (m_ConvertPSOutputToGamma)
    {
        // If manual gamma correction is required, we need to clear the render target with sRGB color
        ClearColor = LinearToSRGB(ClearColor);
    }

    // Let the engine perform required state transitions
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Actualizar la simulación de fluidos si existe (sin renderizar la visualización)
    if (m_pFluidSim)
    {
        // Actualizar simulación interna de fluidos
        m_pFluidSim->Render();
    }

    // Renderizar partículas (sistema original)
    {
        struct Constants
        {
            uint  uiNumParticles;
            float fDeltaTime;
            float fDummy0;
            float fDummy1;

            float2 f2Scale;
            int2   i2ParticleGridSize;
        };
        // Map the buffer and write current world-view-projection matrix
        MapHelper<Constants> ConstData(m_pImmediateContext, m_Constants, MAP_WRITE, MAP_FLAG_DISCARD);
        ConstData->uiNumParticles = static_cast<Uint32>(m_NumParticles);
        ConstData->fDeltaTime     = std::min(m_fTimeDelta, 1.f / 60.f) * m_fSimulationSpeed;

        float  AspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
        float2 f2Scale     = float2(std::sqrt(1.f / AspectRatio), std::sqrt(AspectRatio));
        ConstData->f2Scale = f2Scale;

        int iParticleGridWidth          = static_cast<int>(std::sqrt(static_cast<float>(m_NumParticles)) / f2Scale.x);
        ConstData->i2ParticleGridSize.x = iParticleGridWidth;
        ConstData->i2ParticleGridSize.y = m_NumParticles / iParticleGridWidth;
    }

    // Actualizar la textura de velocidad del fluido en el SRB si existe
    if (m_pFluidSim && m_pMoveParticlesSRB)
    {
        // Obtener textura de velocidad del fluido
        ITextureView* pFluidVelocitySRV = m_pFluidSim->GetVelocitySRV();
        if (pFluidVelocitySRV)
        {
            // Actualizar la variable en el SRB con la textura de velocidad actual
            auto* pFluidVelocityVar = m_pMoveParticlesSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_FluidVelocityTexture");
            if (pFluidVelocityVar)
            {
                pFluidVelocityVar->Set(pFluidVelocitySRV);
            }
        }
    }

    // Viewport para toda la ejecución
    Viewport VP;
    VP.Width    = static_cast<float>(m_pSwapChain->GetDesc().Width);
    VP.Height   = static_cast<float>(m_pSwapChain->GetDesc().Height);
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;
    m_pImmediateContext->SetViewports(1, &VP, 0, 0);

    // Asegurar que las scissor rects estén configuradas correctamente
    Rect scissorRect;
    scissorRect.left   = 0;
    scissorRect.top    = 0;
    scissorRect.right  = static_cast<long>(VP.Width);
    scissorRect.bottom = static_cast<long>(VP.Height);
    m_pImmediateContext->SetScissorRects(1, &scissorRect, 0, 0);

    DispatchComputeAttribs DispatAttribs;
    DispatAttribs.ThreadGroupCountX = (m_NumParticles + m_ThreadGroupSize - 1) / m_ThreadGroupSize;

    m_pImmediateContext->SetPipelineState(m_pResetParticleListsPSO);
    m_pImmediateContext->CommitShaderResources(m_pResetParticleListsSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(DispatAttribs);

    m_pImmediateContext->SetPipelineState(m_pMoveParticlesPSO);
    m_pImmediateContext->CommitShaderResources(m_pMoveParticlesSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(DispatAttribs);

    m_pImmediateContext->SetPipelineState(m_pCollideParticlesPSO);
    m_pImmediateContext->CommitShaderResources(m_pCollideParticlesSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(DispatAttribs);

    m_pImmediateContext->SetPipelineState(m_pUpdateParticleSpeedPSO);
    // Use the same SRB
    m_pImmediateContext->CommitShaderResources(m_pCollideParticlesSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->DispatchCompute(DispatAttribs);

    m_pImmediateContext->SetPipelineState(m_pRenderParticlePSO);
    m_pImmediateContext->CommitShaderResources(m_pRenderParticleSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices  = 4;
    drawAttrs.NumInstances = static_cast<Uint32>(m_NumParticles);
    m_pImmediateContext->Draw(drawAttrs);

    // Renderizar según el modo seleccionado
    if (m_VisualizationMode == VisualizationMode::FLUID_VISUALIZATION)
    {
        // Renderizar visualización del fluido al final (para que aparezca encima)
        if (m_pFluidSim && m_bShowFluidVisualization)
        {
            m_pFluidSim->RenderFluidVisualization(pRTV);
        }
    }
    else if (m_VisualizationMode == VisualizationMode::PAINT_CANVAS)
    {
        // Pintar las partículas al canvas
        PaintParticlesToCanvas();

        // Renderizar el canvas final
        RenderPaintCanvas();
    }
}

void Tutorial14_ComputeShader::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    m_fTimeDelta = static_cast<float>(ElapsedTime);
    m_fAccumulatedTime += m_fTimeDelta;

    // Actualizar sistema de fluidos si existe
    if (m_pFluidSim)
    {
        m_pFluidSim->Update(static_cast<float>(ElapsedTime), m_fSimulationSpeed, m_fViscosity);
    }
}

} // namespace Diligent
