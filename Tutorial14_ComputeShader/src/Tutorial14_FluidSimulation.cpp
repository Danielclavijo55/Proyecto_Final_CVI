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
    VelocityTexDesc.Name                = "Velocity texture 1";
    VelocityTexDesc.Type                = RESOURCE_DIM_TEX_2D;
    VelocityTexDesc.Width               = GRID_SIZE;
    VelocityTexDesc.Height              = GRID_SIZE;
    VelocityTexDesc.Format              = VELOCITY_FORMAT;
    VelocityTexDesc.BindFlags           = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    VelocityTexDesc.ClearValue.Format   = VELOCITY_FORMAT;
    VelocityTexDesc.ClearValue.Color[0] = 0.0f;
    VelocityTexDesc.ClearValue.Color[1] = 0.0f;

    // Inicializar con patrones de fluido más diversos
    std::vector<float> InitialVelocityData(GRID_SIZE * GRID_SIZE * 2, 0.0f);

    // Crear diferentes estructuras de flujo para diversidad de colores
    for (int y = 0; y < GRID_SIZE; y++)
    {
        for (int x = 0; x < GRID_SIZE; x++)
        {
            float fx = static_cast<float>(x) / GRID_SIZE;
            float fy = static_cast<float>(y) / GRID_SIZE;

            // Centro normalizado
            float nx = fx - 0.5f;
            float ny = fy - 0.5f;

            // Para crear diferentes patrones de flujo
            float angle = std::atan2(ny, nx);
            float dist  = std::sqrt(nx * nx + ny * ny);

            // Múltiples patrones superpuestos

            // 1. Vórtex central
            float vx1 = -ny * (0.3f - dist) * 0.1f;
            float vy1 = nx * (0.3f - dist) * 0.1f;
            vx1 *= (dist < 0.3f) ? (0.3f - dist) / 0.3f : 0.0f;
            vy1 *= (dist < 0.3f) ? (0.3f - dist) / 0.3f : 0.0f;

            // 2. Flujo horizontal ondulado
            float vx2 = std::cos(fy * 10.0f) * 0.02f;
            float vy2 = 0.0f;

            // 3. Flujo vertical variado
            float vx3 = 0.0f;
            float vy3 = std::sin(fx * 8.0f) * 0.02f;

            // 4. Patrón diagonal
            float vx4 = std::sin((fx + fy) * 6.0f) * 0.015f;
            float vy4 = std::cos((fx - fy) * 6.0f) * 0.015f;

            // 5. Vórtices pequeños dispersos
            float vx5 = 0.0f;
            float vy5 = 0.0f;

            // Crear 4 vórtices pequeños
            struct MiniVortex
            {
                float x, y, radius, strength;
                bool  clockwise;
            };

            MiniVortex vortices[] = {
                {0.25f, 0.25f, 0.1f, 0.03f, true},
                {0.75f, 0.25f, 0.08f, 0.03f, false},
                {0.25f, 0.75f, 0.08f, 0.03f, false},
                {0.75f, 0.75f, 0.1f, 0.03f, true}};

            for (const auto& v : vortices)
            {
                float vdx   = fx - v.x;
                float vdy   = fy - v.y;
                float vdist = std::sqrt(vdx * vdx + vdy * vdy);

                if (vdist < v.radius)
                {
                    float factor = (v.radius - vdist) / v.radius * v.strength;
                    float dir    = v.clockwise ? -1.0f : 1.0f;
                    vx5 += -vdy * factor * dir;
                    vy5 += vdx * factor * dir;
                }
            }

            // Combinar todos los patrones
            float vx = vx1 + vx2 + vx3 + vx4 + vx5;
            float vy = vy1 + vy2 + vy3 + vy4 + vy5;

            // Almacenar el resultado
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

    // Crear la primera textura con los datos iniciales
    m_pDevice->CreateTexture(VelocityTexDesc, &InitData, &m_pVelocityTexture1);

    // Crear la segunda textura con los mismos datos iniciales
    VelocityTexDesc.Name = "Velocity texture 2";
    m_pDevice->CreateTexture(VelocityTexDesc, &InitData, &m_pVelocityTexture2);

    // Inicializar punteros a vistas de textura
    if (m_pVelocityTexture1 && m_pVelocityTexture2)
    {
        // Configurar vistas iniciales
        ITextureView* pRTV1 = m_pVelocityTexture1->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        ITextureView* pSRV1 = m_pVelocityTexture1->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        ITextureView* pRTV2 = m_pVelocityTexture2->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        ITextureView* pSRV2 = m_pVelocityTexture2->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        // Inicializar al primer buffer como "actual"
        m_CurrentTextureIndex  = 0;
        m_pCurrentVelocityRTV  = pRTV1;
        m_pCurrentVelocitySRV  = pSRV1;
        m_pPreviousVelocitySRV = pSRV2;

        // Guardar vistas para referencia posterior
        m_pVelocityRTV1 = pRTV1;
        m_pVelocitySRV1 = pSRV1;
        m_pVelocityRTV2 = pRTV2;
        m_pVelocitySRV2 = pSRV2;

        // Mantener compatibilidad con código existente
        m_pVelocityRTV = m_pCurrentVelocityRTV;
        m_pVelocitySRV = m_pCurrentVelocitySRV;
    }
    else
    {
        LOG_ERROR_MESSAGE("Failed to create velocity textures");
        throw std::runtime_error("Failed to create velocity textures");
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

            // Obtener dimensiones exactas de la ventana
            float screenWidth  = static_cast<float>(m_pSwapChain->GetDesc().Width);
            float screenHeight = static_cast<float>(m_pSwapChain->GetDesc().Height);

            // Configurar viewport para cubrir exactamente toda la pantalla
            Viewport VP;
            VP.Width    = screenWidth;
            VP.Height   = screenHeight;
            VP.MinDepth = 0.0f;
            VP.MaxDepth = 1.0f;
            VP.TopLeftX = 0.0f;
            VP.TopLeftY = 0.0f;

            m_pContext->SetViewports(1, &VP, 0, 0);

            // Asegurarse que las scissor rects también estén configuradas correctamente
            Rect scissorRect;
            scissorRect.left   = 0;
            scissorRect.top    = 0;
            scissorRect.right  = static_cast<long>(screenWidth);
            scissorRect.bottom = static_cast<long>(screenHeight);
            m_pContext->SetScissorRects(1, &scissorRect, 0, 0);

            // Dibujar quad que cubra exactamente toda la pantalla
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

        // Calcular posición y fuerza circular con menor velocidad de cambio
        float2 forcePos;
        // Reducir la velocidad del movimiento de la fuerza
        forcePos.x = sin(m_Timer * 0.3f) * 0.5f; // Reducido de 0.5 a 0.3
        forcePos.y = cos(m_Timer * 0.3f) * 0.5f;

        float2 force;
        if (m_LastForcePos.x != 0 || m_LastForcePos.y != 0)
        {
            // Reducir la magnitud de la fuerza
            force = (forcePos - m_LastForcePos) * 3.0f; // Reducido de 5.0 a 3.0
        }
        else
        {
            force = float2(0.05f, 0.05f); // Reducido de 0.1 a 0.05
        }

        MapHelper<FluidShaderConstants> Constants(m_pContext, m_pConstantsBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        // Reducir el time step para el fluido para ralentizar el movimiento
        Constants->TimeStep        = deltaTime * simulationSpeed * 0.7f; // Factor adicional de 0.7
        Constants->Viscosity       = viscosity * 1.5f;                   // Aumentar la viscosidad efectiva
        Constants->GridScale       = 1.0f;
        Constants->InverseGridSize = float2(1.0f / GRID_SIZE, 1.0f / GRID_SIZE);
        Constants->ForcePosition   = forcePos;
        Constants->ForceVector     = force;
        Constants->ForceRadius     = 0.18f; // Aumentado de 0.15 a 0.18 para fuerzas más suaves

        m_LastForcePos = forcePos;
    }
}

void Tutorial14_FluidSimulation::Render()
{
    try
    {
        // Paso 1: Aplicar fuerzas al campo de velocidad
        if (m_pForcePSO && m_pForceSRB && m_pCurrentVelocityRTV)
        {
            // Configurar render target - esto renderiza a la textura actual
            ITextureView* pRTVs[] = {m_pCurrentVelocityRTV};
            m_pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Establecer pipeline y recursos
            m_pContext->SetPipelineState(m_pForcePSO);
            m_pContext->CommitShaderResources(m_pForceSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Dibujar quad para aplicar fuerzas
            DrawFullScreenQuad();
        }

        // Intercambiar texturas tras aplicar fuerzas
        SwapVelocityTextures();

        // Paso 2: Advección del campo de velocidad
        if (m_pAdvectionPSO && m_pAdvectionSRB && m_pCurrentVelocityRTV)
        {
            // Configurar render target
            ITextureView* pRTVs[] = {m_pCurrentVelocityRTV};
            m_pContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Establecer pipeline y recursos
            m_pContext->SetPipelineState(m_pAdvectionPSO);
            m_pContext->CommitShaderResources(m_pAdvectionSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Dibujar quad para advección
            DrawFullScreenQuad();
        }

        // Intercambiar texturas tras advección
        SwapVelocityTextures();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR_MESSAGE("Error in Tutorial14_FluidSimulation::Render: %s", e.what());
    }
}

void Tutorial14_FluidSimulation::DrawFullScreenQuad()
{
    // Configurar viewport para cubrir toda la pantalla
    Viewport VP;
    VP.Width    = static_cast<float>(GRID_SIZE); 
    VP.Height   = static_cast<float>(GRID_SIZE); 
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

void Tutorial14_FluidSimulation::SwapVelocityTextures()
{
    m_CurrentTextureIndex = 1 - m_CurrentTextureIndex;

    if (m_CurrentTextureIndex == 0)
    {
        m_pCurrentVelocityRTV  = m_pVelocityRTV1;
        m_pCurrentVelocitySRV  = m_pVelocitySRV1;
        m_pPreviousVelocitySRV = m_pVelocitySRV2;
    }
    else
    {
        m_pCurrentVelocityRTV  = m_pVelocityRTV2;
        m_pCurrentVelocitySRV  = m_pVelocitySRV2;
        m_pPreviousVelocitySRV = m_pVelocitySRV1;
    }

    // Actualizar m_pVelocityRTV y m_pVelocitySRV para mantener compatibilidad con el código existente
    m_pVelocityRTV = m_pCurrentVelocityRTV;
    m_pVelocitySRV = m_pCurrentVelocitySRV;

    // Recrear los SRBs con las nuevas texturas
    RecreateShaderResourceBindings();
}

void Tutorial14_FluidSimulation::UpdateTextureBindings()
{
    // Actualizar binding para shader de advección
    if (m_pAdvectionSRB)
    {
        auto* pVelocityVar = m_pAdvectionSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_VelocityTexture");
        if (pVelocityVar)
        {
            pVelocityVar->Set(m_pPreviousVelocitySRV); // Usamos la textura previa para leer
        }
    }

    // Actualizar binding para shader de fuerzas
    if (m_pForceSRB)
    {
        auto* pVelocityVar = m_pForceSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_VelocityTexture");
        if (pVelocityVar)
        {
            pVelocityVar->Set(m_pPreviousVelocitySRV); // Usamos la textura previa para leer
        }
    }

    // Actualizar binding para shader de visualización
    if (m_pVisualizationSRB)
    {
        auto* pVelocityVar = m_pVisualizationSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_VelocityTexture");
        if (pVelocityVar)
        {
            pVelocityVar->Set(m_pCurrentVelocitySRV); // Usamos la textura actual para visualizar
        }
    }
}

void Tutorial14_FluidSimulation::RecreateShaderResourceBindings()
{
    // Recrear SRB para advección
    if (m_pAdvectionPSO)
    {
        m_pAdvectionSRB.Release();
        m_pAdvectionPSO->CreateShaderResourceBinding(&m_pAdvectionSRB, true);

        if (m_pAdvectionSRB)
        {
            auto* pVelocityVar = m_pAdvectionSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_VelocityTexture");
            if (pVelocityVar)
            {
                pVelocityVar->Set(m_pPreviousVelocitySRV);
            }
            else
            {
                LOG_ERROR_MESSAGE("Variable 'g_VelocityTexture' not found in advection shader");
            }

            // Reconfigurar sampler
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
        else
        {
            LOG_ERROR_MESSAGE("Failed to create advection SRB");
        }
    }

    // Recrear SRB para fuerzas
    if (m_pForcePSO)
    {
        m_pForceSRB.Release();
        m_pForcePSO->CreateShaderResourceBinding(&m_pForceSRB, true);

        if (m_pForceSRB)
        {
            auto* pVelocityVar = m_pForceSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_VelocityTexture");
            if (pVelocityVar)
            {
                pVelocityVar->Set(m_pPreviousVelocitySRV);
            }
            else
            {
                LOG_ERROR_MESSAGE("Variable 'g_VelocityTexture' not found in force shader");
            }

            // Reconfigurar sampler
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
            else
            {
                LOG_ERROR_MESSAGE("Variable 'g_LinearSampler' not found in force shader");
            }

            // Vincular buffer de constantes
            auto* pConstantsVar = m_pForceSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbFluidConstants");
            if (pConstantsVar)
            {
                pConstantsVar->Set(m_pConstantsBuffer);
            }
            else
            {
                LOG_ERROR_MESSAGE("Variable 'cbFluidConstants' not found in force shader");
            }
        }
        else
        {
            LOG_ERROR_MESSAGE("Failed to create force SRB");
        }
    }

    // Recrear SRB para visualización
    if (m_pVisualizationPSO)
    {
        m_pVisualizationSRB.Release();
        m_pVisualizationPSO->CreateShaderResourceBinding(&m_pVisualizationSRB, true);

        if (m_pVisualizationSRB)
        {
            auto* pVelocityVar = m_pVisualizationSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_VelocityTexture");
            if (pVelocityVar)
            {
                pVelocityVar->Set(m_pCurrentVelocitySRV); // ¡Importante! Usamos la textura actual para visualización
            }
            else
            {
                LOG_ERROR_MESSAGE("Variable 'g_VelocityTexture' not found in visualization shader");
            }

            // Reconfigurar sampler
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
            else
            {
                LOG_ERROR_MESSAGE("Variable 'g_LinearSampler' not found in visualization shader");
            }

            // Nota: El shader de visualización podría no necesitar el buffer de constantes
            // pero lo añadimos por consistencia
            auto* pConstantsVar = m_pVisualizationSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbFluidConstants");
            if (pConstantsVar)
            {
                pConstantsVar->Set(m_pConstantsBuffer);
            }
        }
        else
        {
            LOG_ERROR_MESSAGE("Failed to create visualization SRB");
        }
    }
}

float2 Tutorial14_FluidSimulation::GetVelocityAt(const float2& position)
{
    // Convertir posición al espacio de la textura [-1,1] -> [0,1]
    float2 texCoord;
    texCoord.x = (position.x + 1.0f) * 0.5f;
    texCoord.y = (position.y + 1.0f) * 0.5f;

    // Verificar si la posición está dentro de los límites
    if (texCoord.x < 0.0f || texCoord.x > 1.0f || texCoord.y < 0.0f || texCoord.y > 1.0f)
        return float2(0, 0);

    // Simplificación: En lugar de leer de la textura, que es complicado en CPU,
    // usaremos un cálculo simplificado basado en la posición para simular
    // que estamos obteniendo velocidad del campo de fluido

    float  time = m_Timer * 0.5f;
    float2 velocity;

    // Usar la posición como base para simular un campo de velocidad
    velocity.x = sin(texCoord.x * 3.1415f * 2.0f + time) * cos(texCoord.y * 3.1415f * 3.0f + time * 1.3f) * 0.3f;
    velocity.y = cos(texCoord.x * 3.1415f * 2.5f + time * 1.2f) * sin(texCoord.y * 3.1415f * 2.0f + time) * 0.3f;

    // Añadir un remolino mayor en el centro
    float2 center       = texCoord - float2(0.5f, 0.5f);
    float  distToCenter = length(center);
    if (distToCenter < 0.3f)
    {
        float  strength = (0.3f - distToCenter) / 0.3f;
        float2 swirl    = float2(-center.y, center.x) * strength * 0.6f;
        velocity += swirl;
    }

    return velocity;
}

} // namespace Diligent