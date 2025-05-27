#pragma once

#include "SampleBase.hpp"
#include "ResourceMapping.h"
#include "BasicMath.hpp"
#include <memory>
#include "Tutorial14_FluidSimulation.hpp"

namespace Diligent
{

enum class VisualizationMode
{
    FLUID_VISUALIZATION, // Sistema actual con fluidos visibles
    PAINT_CANVAS         // Solo el canvas pintado
};

class Tutorial14_ComputeShader final : public SampleBase
{
public:
    virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override final;

    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial14: Compute Shader"; }

private:
    void CreateRenderParticlePSO();
    void CreateUpdateParticlePSO();
    void CreateParticleBuffers();
    void CreateConsantBuffer();
    void UpdateUI();

    // Paint System Methods
    void CreatePaintSystem();
    void CreateCanvasTexture();
    void CreateColorPalette();
    void CreatePaintPipelines();
    void RenderPaintCanvas();
    void PaintParticlesToCanvas();
    void ClearCanvas();
    void RecreatePaintSRB();

    // Sistema de fluidos independiente
    std::unique_ptr<Tutorial14_FluidSimulation> m_pFluidSim;

    // Paint System Variables
    VisualizationMode m_VisualizationMode = VisualizationMode::FLUID_VISUALIZATION;

    // Paint System Resources
    RefCntAutoPtr<ITexture>     m_pCanvasTexture;
    RefCntAutoPtr<ITextureView> m_pCanvasRTV;
    RefCntAutoPtr<ITextureView> m_pCanvasSRV;

    RefCntAutoPtr<ITexture>     m_pColorPaletteTexture;
    RefCntAutoPtr<ITextureView> m_pColorPaletteSRV;

    // Paint Pipelines
    RefCntAutoPtr<IPipelineState>         m_pPaintParticlePSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pPaintParticleSRB;
    RefCntAutoPtr<IPipelineState>         m_pRenderCanvasPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pRenderCanvasSRB;

    RefCntAutoPtr<IBuffer> m_pPaintConstants;

    // Variable para controlar visualización de fluidos
    bool m_bShowFluidVisualization = true;

    // Variable para viscosidad del fluido
    float m_fViscosity = 0.1f;

    int m_NumParticles    = 2000;
    int m_ThreadGroupSize = 256;

    RefCntAutoPtr<IPipelineState>         m_pRenderParticlePSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pRenderParticleSRB;
    RefCntAutoPtr<IPipelineState>         m_pResetParticleListsPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pResetParticleListsSRB;
    RefCntAutoPtr<IPipelineState>         m_pMoveParticlesPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pMoveParticlesSRB;
    RefCntAutoPtr<IPipelineState>         m_pCollideParticlesPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pCollideParticlesSRB;
    RefCntAutoPtr<IPipelineState>         m_pUpdateParticleSpeedPSO;
    RefCntAutoPtr<IBuffer>                m_Constants;
    RefCntAutoPtr<IBuffer>                m_pParticleAttribsBuffer;
    RefCntAutoPtr<IBuffer>                m_pParticleListsBuffer;
    RefCntAutoPtr<IBuffer>                m_pParticleListHeadsBuffer;
    RefCntAutoPtr<IResourceMapping>       m_pResMapping;

    float m_fTimeDelta       = 0;
    float m_fSimulationSpeed = 1;
    float m_fAccumulatedTime = 0;
};

} // namespace Diligent