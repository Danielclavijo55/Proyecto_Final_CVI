#pragma once

#include "BasicMath.hpp"
#include "MapHelper.hpp"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "EngineFactory.h"
#include "SwapChain.h"

namespace Diligent
{

class Tutorial14_FluidSimulation
{
public:
    Tutorial14_FluidSimulation(IRenderDevice*  pDevice,
                               IDeviceContext* pContext,
                               IEngineFactory* pEngineFactory,
                               ISwapChain*     pSwapChain);

    // Destructor declarado explícitamente
    ~Tutorial14_FluidSimulation();

    void Update(float deltaTime, float simulationSpeed, float viscosity);
    void Render();

    // Método para renderizar visualización al backbuffer
    void RenderFluidVisualization(ITextureView* pRTV);

    // Método para consultar velocidad en una posición específica
    float2 GetVelocityAt(const float2& position);

    // Getter para la textura de velocidad
    ITextureView* GetVelocitySRV() const { return m_pVelocitySRV; }

private:
    // Constantes
    static constexpr Uint32         GRID_SIZE       = 256;
    static constexpr TEXTURE_FORMAT VELOCITY_FORMAT = TEX_FORMAT_RG32_FLOAT;

    // Métodos de inicialización
    void CreateTextures();
    void CreateConstantsBuffer();
    void CreatePipelines();
    void DrawFullScreenQuad();

    // Visualización
    void RenderFluidVisualizationInternal();


    //Funciones
    void SwapVelocityTextures();
    void UpdateTextureBindings();
    void RecreateShaderResourceBindings();

    // Dispositivos de renderizado
    IRenderDevice*  m_pDevice        = nullptr;
    IDeviceContext* m_pContext       = nullptr;
    IEngineFactory* m_pEngineFactory = nullptr;
    ISwapChain*     m_pSwapChain     = nullptr; // Ahora guardamos una referencia al SwapChain

    // Recursos de fluidos
    RefCntAutoPtr<ITexture> m_pVelocityTexture;
    RefCntAutoPtr<ITexture> m_pVelocityTexture1;
    RefCntAutoPtr<ITexture> m_pVelocityTexture2;
    RefCntAutoPtr<IBuffer>  m_pConstantsBuffer;
    RefCntAutoPtr<ITexture> m_pStagingTexture;

    // Referencias a vistas de textura actual y anterior
    ITextureView* m_pCurrentVelocityRTV  = nullptr;
    ITextureView* m_pCurrentVelocitySRV  = nullptr;
    ITextureView* m_pPreviousVelocitySRV = nullptr;

    // Mantener referencias específicas a cada textura para facilitar el intercambio
    ITextureView* m_pVelocityRTV1 = nullptr;
    ITextureView* m_pVelocitySRV1 = nullptr;
    ITextureView* m_pVelocityRTV2 = nullptr;
    ITextureView* m_pVelocitySRV2 = nullptr;

    // Índice de textura actual (0 o 1)
    int m_CurrentTextureIndex = 0;

    // Vistas de textura
    ITextureView* m_pVelocityRTV = nullptr;
    ITextureView* m_pVelocitySRV = nullptr;

    // Pipeline state y SRB para advección
    RefCntAutoPtr<IPipelineState>         m_pAdvectionPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pAdvectionSRB;

    // Pipeline state y SRB para aplicación de fuerzas
    RefCntAutoPtr<IPipelineState>         m_pForcePSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pForceSRB;

    // Pipeline state y SRB para visualización
    RefCntAutoPtr<IPipelineState>         m_pVisualizationPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pVisualizationSRB;

    // Variables de simulación
    float  m_Timer        = 0.0f;
    float2 m_LastForcePos = float2(0, 0);
};

} // namespace Diligent