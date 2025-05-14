#include "Tutorial14_FluidSimulation.hpp"
#include "GraphicsTypes.h"
#include "ShaderMacroHelper.hpp"
#include "RefCntAutoPtr.hpp"
#include <random> // Añadir para usar mt19937 y uniform_real_distribution

namespace Diligent
{

Tutorial14_FluidSimulation::Tutorial14_FluidSimulation(IRenderDevice*  pDevice,
                                                       IDeviceContext* pContext,
                                                       IEngineFactory* pEngineFactory,
                                                       ISwapChain*     pSwapChain) :
    m_pDevice(pDevice),
    m_pContext(pContext),
    m_pEngineFactory(pEngineFactory),
    m_pSwapChain(pSwapChain) // Guardar el SwapChain
{
    try
    {
        CreateConstantsBuffer();
        CreateTextures();
        CreatePipelines();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR_MESSAGE("Error in Tutorial14_FluidSimulation initialization: %s", e.what());
        throw;
    }
}

// Definir el destructor correctamente
Tutorial14_FluidSimulation::~Tutorial14_FluidSimulation()
{
    // Los recursos se liberan automáticamente por RefCntAutoPtr
    LOG_INFO_MESSAGE("Tutorial14_FluidSimulation destroyed");
}

// Inicialización mejorada del campo de velocidad
void Tutorial14_FluidSimulation::CreateTextures()
{
    // Crear textura de velocidad con valores iniciales
    TextureDesc VelocityTexDesc;
    VelocityTexDesc.Name                = "Velocity texture";
    VelocityTexDesc.Type                = RESOURCE_DIM_TEX_2D;
    VelocityTexDesc.Width               = GRID_SIZE;
    VelocityTexDesc.Height              = GRID_SIZE;
    VelocityTexDesc.Format              = VELOCITY_FORMAT;
    VelocityTexDesc.BindFlags           = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    VelocityTexDesc.ClearValue.Format   = VELOCITY_FORMAT;
    VelocityTexDesc.ClearValue.Color[0] = 0.0f;
    VelocityTexDesc.ClearValue.Color[1] = 0.0f;

    // Inicializar con múltiples remolinos para crear un patrón más interesante
    std::vector<float> InitialVelocityData(GRID_SIZE * GRID_SIZE * 2, 0.0f);

    // Múltiples centros de vórtice para un patrón más interesante
    struct Vortex
    {
        float centerX;
        float centerY;
        float radius;
        float strength;
        bool  clockwise;
    };

    // Definir varios vórtices a través del campo
    std::vector<Vortex> vortices = {
        {GRID_SIZE * 0.5f, GRID_SIZE * 0.5f, GRID_SIZE * 0.4f, 0.15f, true},   // Vórtice central
        {GRID_SIZE * 0.25f, GRID_SIZE * 0.25f, GRID_SIZE * 0.2f, 0.1f, false}, // Esquina superior izquierda
        {GRID_SIZE * 0.75f, GRID_SIZE * 0.25f, GRID_SIZE * 0.2f, 0.12f, true}, // Esquina superior derecha
        {GRID_SIZE * 0.25f, GRID_SIZE * 0.75f, GRID_SIZE * 0.2f, 0.12f, true}, // Esquina inferior izquierda
        {GRID_SIZE * 0.75f, GRID_SIZE * 0.75f, GRID_SIZE * 0.2f, 0.1f, false}  // Esquina inferior derecha
    };

    // Crear un campo de velocidad combinando varios vórtices
    for (int y = 0; y < GRID_SIZE; y++)
    {
        for (int x = 0; x < GRID_SIZE; x++)
        {
            float vx = 0.0f;
            float vy = 0.0f;

            // Sumar la contribución de cada vórtice
            for (const auto& vortex : vortices)
            {
                // Calcular posición relativa al centro del vórtice
                float dx     = static_cast<float>(x) - vortex.centerX;
                float dy     = static_cast<float>(y) - vortex.centerY;
                float radius = std::sqrt(dx * dx + dy * dy);

                // Velocidad basada en el vórtice
                if (radius < vortex.radius)
                {
                    float angle          = std::atan2(dy, dx);
                    float velocityFactor = (1.0f - radius / vortex.radius) * vortex.strength;

                    // Componentes de velocidad para el vórtice, con dirección según clockwise
                    float direction = vortex.clockwise ? -1.0f : 1.0f;
                    float vortexVx  = -std::sin(angle) * velocityFactor * direction;
                    float vortexVy  = std::cos(angle) * velocityFactor * direction;

                    // Acumular la contribución de este vórtice
                    vx += vortexVx;
                    vy += vortexVy;
                }
            }

            // Añadir un campo de fondo suave
            float fieldStrength = 0.02f;
            float fieldVx       = std::sin(y * 0.1f) * fieldStrength;
            float fieldVy       = std::cos(x * 0.1f) * fieldStrength;

            vx += fieldVx;
            vy += fieldVy;

            // Guardar el resultado
            int index                      = (y * GRID_SIZE + x) * 2;
            InitialVelocityData[index]     = vx;
            InitialVelocityData[index + 1] = vy;
        }
    }

    TextureData       InitData;
    TextureSubResData SubResData;
    SubResData.pData         = InitialVelocityData.data();
    SubResData.Stride        = GRID_SIZE * 2 * sizeof(float);
    InitData.pSubResources   = &SubResData;
    InitData.NumSubresources = 1;

    m_pDevice->CreateTexture(VelocityTexDesc, &InitData, &m_pVelocityTexture);
    if (m_pVelocityTexture)
    {
        m_pVelocityRTV = m_pVelocityTexture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        m_pVelocitySRV = m_pVelocityTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
    else
    {
        LOG_ERROR_MESSAGE("Failed to create velocity texture");
        throw std::runtime_error("Failed to create velocity texture");
    }
}

void Tutorial14_FluidSimulation::CreateConstantsBuffer()
{
    struct FluidShaderConstants
    {
        float TimeStep;
        float Viscosity;
        float GridScale;
        float Padding0;

        float2 InverseGridSize;
        float2 ForcePosition;

        float2 ForceVector;
        float  ForceRadius;
        float  Padding1;
    };

    BufferDesc BuffDesc;
    BuffDesc.Name           = "Fluid constants buffer";
    BuffDesc.Usage          = USAGE_DYNAMIC;
    BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
    BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    BuffDesc.Size           = sizeof(FluidShaderConstants);

    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pConstantsBuffer);
    if (!m_pConstantsBuffer)
    {
        LOG_ERROR_MESSAGE("Failed to create fluid constants buffer");
    }
}

void Tutorial14_FluidSimulation::CreatePipelines()
{
    // Crear shader factory
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

    // Configuración de shader
    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.pShaderSourceStreamFactory      = pShaderSourceFactory;

    // Vertex shader fullscreen quad
    RefCntAutoPtr<IShader> pFullScreenQuadVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Full-screen quad VS";
        ShaderCI.FilePath        = "FluidVertexShader.fx";
        m_pDevice->CreateShader(ShaderCI, &pFullScreenQuadVS);
        if (!pFullScreenQuadVS)
        {
            LOG_ERROR_MESSAGE("Failed to create fluid vertex shader");
            return;
        }
    }

    // Pixel shader para advección
    RefCntAutoPtr<IShader> pAdvectionPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Advection PS";
        ShaderCI.FilePath        = "FluidPixelShader.fx";
        m_pDevice->CreateShader(ShaderCI, &pAdvectionPS);
        if (!pAdvectionPS)
        {
            LOG_ERROR_MESSAGE("Failed to create advection pixel shader");
            return;
        }
    }

    // Pixel shader para fuerzas
    RefCntAutoPtr<IShader> pForcePS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Force PS";
        ShaderCI.FilePath        = "FluidForceShader.fx";
        m_pDevice->CreateShader(ShaderCI, &pForcePS);
        if (!pForcePS)
        {
            LOG_ERROR_MESSAGE("Failed to create force pixel shader");
            return;
        }
    }

    // Configurar PSO para advección
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name         = "Advection PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.pVS = pFullScreenQuadVS;
    PSOCreateInfo.pPS = pAdvectionPS;

    auto& GraphicsPipeline             = PSOCreateInfo.GraphicsPipeline;
    GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    GraphicsPipeline.NumRenderTargets  = 1;
    GraphicsPipeline.RTVFormats[0]     = VELOCITY_FORMAT;
    GraphicsPipeline.DSVFormat         = TEX_FORMAT_UNKNOWN;

    auto& BlendDesc                        = GraphicsPipeline.BlendDesc;
    BlendDesc.RenderTargets[0].BlendEnable = False;

    auto& RasterizerDesc    = GraphicsPipeline.RasterizerDesc;
    RasterizerDesc.CullMode = CULL_MODE_NONE;

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pAdvectionPSO);

    if (m_pAdvectionPSO)
    {
        m_pAdvectionPSO->CreateShaderResourceBinding(&m_pAdvectionSRB, true);

        if (m_pVelocitySRV && m_pAdvectionSRB)
        {
            auto* pVelocityVar = m_pAdvectionSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_VelocityTexture");
            if (pVelocityVar)
            {
                pVelocityVar->Set(m_pVelocitySRV);
            }
            else
            {
                LOG_ERROR_MESSAGE("Variable 'g_VelocityTexture' not found in advection shader");
            }

            // Configurar sampler para advección
            SamplerDesc SamDesc;
            SamDesc.MinFilter = FILTER_TYPE_LINEAR;
            SamDesc.MagFilter = FILTER_TYPE_LINEAR;
            SamDesc.MipFilter = FILTER_TYPE_LINEAR;
            SamDesc.AddressU  = TEXTURE_ADDRESS_CLAMP;
            SamDesc.AddressV  = TEXTURE_ADDRESS_CLAMP;
            SamDesc.AddressW  = TEXTURE_ADDRESS_CLAMP;

            RefCntAutoPtr<ISampler> pSampler;
            m_pDevice->CreateSampler(SamDesc, &pSampler);

            auto* pSamplerVar = m_pAdvectionSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_LinearSampler");
            if (pSamplerVar)
            {
                pSamplerVar->Set(pSampler);
            }
            else
            {
                LOG_ERROR_MESSAGE("Variable 'g_LinearSampler' not found in advection shader");
            }

            // Vincular buffer de constantes
            auto* pConstantsVar = m_pAdvectionSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbFluidConstants");
            if (pConstantsVar)
            {
                pConstantsVar->Set(m_pConstantsBuffer);
            }
            else
            {
                LOG_ERROR_MESSAGE("Variable 'cbFluidConstants' not found in advection shader");
            }
        }
    }
    else
    {
        LOG_ERROR_MESSAGE("Failed to create advection PSO");
    }

    // Configurar PSO para fuerzas
    PSOCreateInfo.PSODesc.Name = "Force PSO";
    PSOCreateInfo.pPS          = pForcePS;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pForcePSO);

    if (m_pForcePSO)
    {
        m_pForcePSO->CreateShaderResourceBinding(&m_pForceSRB, true);

        if (m_pVelocitySRV && m_pForceSRB)
        {
            auto* pVelocityVar = m_pForceSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_VelocityTexture");
            if (pVelocityVar)
            {
                pVelocityVar->Set(m_pVelocitySRV);
            }

            // Configurar sampler para fuerzas
            SamplerDesc SamDesc;
            SamDesc.MinFilter = FILTER_TYPE_LINEAR;
            SamDesc.MagFilter = FILTER_TYPE_LINEAR;
            SamDesc.MipFilter = FILTER_TYPE_LINEAR;
            SamDesc.AddressU  = TEXTURE_ADDRESS_CLAMP;
            SamDesc.AddressV  = TEXTURE_ADDRESS_CLAMP;
            SamDesc.AddressW  = TEXTURE_ADDRESS_CLAMP;

            RefCntAutoPtr<ISampler> pSampler;
            m_pDevice->CreateSampler(SamDesc, &pSampler);

            auto* pSamplerVar = m_pForceSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_LinearSampler");
            if (pSamplerVar)
            {
                pSamplerVar->Set(pSampler);
            }

            // Vincular buffer de constantes
            auto* pConstantsVar = m_pForceSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbFluidConstants");
            if (pConstantsVar)
            {
                pConstantsVar->Set(m_pConstantsBuffer);
            }
        }
    }
    else
    {
        LOG_ERROR_MESSAGE("Failed to create force PSO");
    }

    // Pixel shader para visualización
    RefCntAutoPtr<IShader> pVisualizationPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Visualization PS";
        ShaderCI.FilePath        = "FluidVisualizationShader.fx";
        m_pDevice->CreateShader(ShaderCI, &pVisualizationPS);
        if (!pVisualizationPS)
        {
            LOG_ERROR_MESSAGE("Failed to create visualization pixel shader");
            return;
        }
    }

    // Configurar PSO para visualización
    PSOCreateInfo.PSODesc.Name = "Visualization PSO";
    PSOCreateInfo.pPS          = pVisualizationPS;

    // Habilitar blending para la visualización
    BlendDesc.RenderTargets[0].BlendEnable = True;
    BlendDesc.RenderTargets[0].SrcBlend    = BLEND_FACTOR_SRC_ALPHA;
    BlendDesc.RenderTargets[0].DestBlend   = BLEND_FACTOR_INV_SRC_ALPHA;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pVisualizationPSO);

    if (m_pVisualizationPSO)
    {
        m_pVisualizationPSO->CreateShaderResourceBinding(&m_pVisualizationSRB, true);

        if (m_pVelocitySRV && m_pVisualizationSRB)
        {
            auto* pVelocityVar = m_pVisualizationSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_VelocityTexture");
            if (pVelocityVar)
            {
                pVelocityVar->Set(m_pVelocitySRV);
            }

            // Configurar sampler para visualización
            SamplerDesc SamDesc;
            SamDesc.MinFilter = FILTER_TYPE_LINEAR;
            SamDesc.MagFilter = FILTER_TYPE_LINEAR;
            SamDesc.MipFilter = FILTER_TYPE_LINEAR;
            SamDesc.AddressU  = TEXTURE_ADDRESS_CLAMP;
            SamDesc.AddressV  = TEXTURE_ADDRESS_CLAMP;
            SamDesc.AddressW  = TEXTURE_ADDRESS_CLAMP;

            RefCntAutoPtr<ISampler> pSampler;
            m_pDevice->CreateSampler(SamDesc, &pSampler);

            auto* pSamplerVar = m_pVisualizationSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_LinearSampler");
            if (pSamplerVar)
            {
                pSamplerVar->Set(pSampler);
            }
        }
    }
    else
    {
        LOG_ERROR_MESSAGE("Failed to create visualization PSO");
    }
}

// Ajustar el método RenderFluidVisualization para cubrir mejor la pantalla
void Tutorial14_FluidSimulation::RenderFluidVisualization(ITextureView* pRTV)
{
    try
    {
        if (m_pVisualizationPSO && m_pVisualizationSRB && pRTV)
        {
            // Configurar render target con el RTV proporcionado
            m_pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Establecer pipeline y recursos
            m_pContext->SetPipelineState(m_pVisualizationPSO);
            m_pContext->CommitShaderResources(m_pVisualizationSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Dibujar quad para visualización - ahora cubriendo toda la pantalla
            Viewport VP;
            float    screenWidth  = static_cast<float>(m_pSwapChain->GetDesc().Width);
            float    screenHeight = static_cast<float>(m_pSwapChain->GetDesc().Height);

            VP.Width    = screenWidth;
            VP.Height   = screenHeight;
            VP.MinDepth = 0.0f;
            VP.MaxDepth = 1.0f;
            VP.TopLeftX = 0.0f;
            VP.TopLeftY = 0.0f;

            m_pContext->SetViewports(1, &VP, 0, 0);

            DrawAttribs drawAttrs;
            drawAttrs.NumVertices = 4;
            m_pContext->Draw(drawAttrs);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR_MESSAGE("Error in Tutorial14_FluidSimulation::RenderFluidVisualization: %s", e.what());
    }
}

void Tutorial14_FluidSimulation::Update(float deltaTime, float simulationSpeed, float viscosity)
{
    m_Timer += deltaTime;

    // Actualizar buffer de constantes
    if (m_pConstantsBuffer)
    {
        struct FluidShaderConstants
        {
            float TimeStep;
            float Viscosity;
            float GridScale;
            float Padding0;

            float2 InverseGridSize;
            float2 ForcePosition;

            float2 ForceVector;
            float  ForceRadius;
            float  Padding1;
        };

        // Calcular posición y fuerza circular
        float2 forcePos;
        forcePos.x = sin(m_Timer * 0.5f) * 0.5f;
        forcePos.y = cos(m_Timer * 0.5f) * 0.5f;

        float2 force;
        if (m_LastForcePos.x != 0 || m_LastForcePos.y != 0)
        {
            force = (forcePos - m_LastForcePos) * 5.0f;
        }
        else
        {
            force = float2(0.1f, 0.1f);
        }

        MapHelper<FluidShaderConstants> Constants(m_pContext, m_pConstantsBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        Constants->TimeStep        = deltaTime * simulationSpeed;
        Constants->Viscosity       = viscosity;
        Constants->GridScale       = 1.0f;
        Constants->InverseGridSize = float2(1.0f / GRID_SIZE, 1.0f / GRID_SIZE);
        Constants->ForcePosition   = forcePos;
        Constants->ForceVector     = force;
        Constants->ForceRadius     = 0.15f;

        m_LastForcePos = forcePos;
    }
}

void Tutorial14_FluidSimulation::Render()
{
    try
    {
        // Paso 1: Aplicar fuerzas al campo de velocidad
        if (m_pForcePSO && m_pForceSRB && m_pVelocityRTV)
        {
            // Configurar render target - esto renderiza a la textura interna, no a pantalla
            ITextureView* pRTVs[] = {m_pVelocityRTV};
            m_pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Establecer pipeline y recursos
            m_pContext->SetPipelineState(m_pForcePSO);
            m_pContext->CommitShaderResources(m_pForceSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Dibujar quad para aplicar fuerzas
            DrawFullScreenQuad();
        }

        // Paso 2: Advección del campo de velocidad
        if (m_pAdvectionPSO && m_pAdvectionSRB && m_pVelocityRTV)
        {
            // Configurar render target
            ITextureView* pRTVs[] = {m_pVelocityRTV};
            m_pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Establecer pipeline y recursos
            m_pContext->SetPipelineState(m_pAdvectionPSO);
            m_pContext->CommitShaderResources(m_pAdvectionSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Dibujar quad para advección
            DrawFullScreenQuad();
        }

        // Paso 3: No renderizamos la visualización aquí - se hará desde la clase principal
        // La clase principal llamará a RenderFluidVisualization y le pasará el RTV correcto
    }
    catch (const std::exception& e)
    {
        LOG_ERROR_MESSAGE("Error in Tutorial14_FluidSimulation::Render: %s", e.what());
    }
}

void Tutorial14_FluidSimulation::DrawFullScreenQuad()
{
    // Configurar viewport pero usar dimensiones más pequeñas o ubicación diferente
    // para no cubrir toda la pantalla durante la depuración
    Viewport VP;
    VP.Width    = static_cast<float>(GRID_SIZE) / 2; // Reducir a la mitad para pruebas
    VP.Height   = static_cast<float>(GRID_SIZE) / 2;
    VP.MinDepth = 0.0f;
    VP.MaxDepth = 1.0f;
    VP.TopLeftX = 0.0f;
    VP.TopLeftY = 0.0f;

    m_pContext->SetViewports(1, &VP, 0, 0);

    // Dibujar un quad
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    m_pContext->Draw(drawAttrs);
}

float2 Tutorial14_FluidSimulation::GetVelocityAt(const float2& position)
{
    // Este método podría implementarse para permitir que el sistema de partículas
    // consulte la velocidad del fluido en una posición específica
    // Por ahora, devolvemos una velocidad simulada basada en el tiempo
    float2 velocity;
    velocity.x = sin(position.x * 3.0f + m_Timer) * 0.3f;
    velocity.y = cos(position.y * 3.0f + m_Timer) * 0.3f;
    return velocity;
}

} // namespace Diligent