/*     Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "ShadowsSample.h"
#include "MapHelper.h"
#include "FileSystem.h"
#include "ShaderMacroHelper.h"
#include "CommonlyUsedStates.h"
#include "StringTools.h"
#include "GraphicsUtilities.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new ShadowsSample();
}

ShadowsSample::~ShadowsSample()
{
}

void ShadowsSample::GetEngineInitializationAttribs(DeviceType         DevType,
                                                   EngineCreateInfo&  Attribs)
{
    SampleBase::GetEngineInitializationAttribs(DevType, Attribs);
#if VULKAN_SUPPORTED
    if(DevType == DeviceType::Vulkan)
    {
        auto& VkAttrs = static_cast<EngineVkCreateInfo&>(Attribs);
        VkAttrs.EnabledFeatures.samplerAnisotropy = true;
        VkAttrs.EnabledFeatures.depthClamp        = true;
    }
#endif
}

void ShadowsSample::Initialize(IEngineFactory* pEngineFactory, IRenderDevice *pDevice, IDeviceContext **ppContexts, Uint32 NumDeferredCtx, ISwapChain *pSwapChain)
{
    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);
    std::string MeshFileName = "Powerplant/Powerplant.sdkmesh";
    m_Mesh.Create(MeshFileName.c_str());
    std::string Directory;
    FileSystem::SplitFilePath(MeshFileName, &Directory, nullptr);
    m_Mesh.LoadGPUResources(Directory.c_str(), pDevice, m_pImmediateContext);

    CreateUniformBuffer(pDevice, sizeof(CameraAttribs), "Camera attribs buffer", &m_CameraAttribsCB);
    CreateUniformBuffer(pDevice, sizeof(LightAttribs), "Light attribs buffer", &m_LightAttribsCB);
    CreatePipelineStates(pDevice);

    m_LightAttribs.ShadowAttribs.iNumCascades = 4;
    m_LightAttribs.ShadowAttribs.fFixedDepthBias = 1e-3f;
    m_LightAttribs.f4Direction    = float3(0.753204405f, -0.243520901f, -0.611060560f);
    m_LightAttribs.f4Intensity    = float4(1, 1, 1, 1);
    m_LightAttribs.f4AmbientLight = float4(0.2f, 0.2f, 0.2f, 1);

    m_Camera.SetPos(float3(70, 10, 0.f));
    m_Camera.SetRotation(-PI_F/2.f, 0);
    m_Camera.SetRotationSpeed(0.005f);
    m_Camera.SetMoveSpeed(5.f);
    m_Camera.SetSpeedUpScales(5.f, 10.f);

    CreateShadowMap();
}



void ShadowsSample::DXSDKMESH_VERTEX_ELEMENTtoInputLayoutDesc(const DXSDKMESH_VERTEX_ELEMENT*  VertexElement,
                                                              Uint32                           Stride,
                                                              InputLayoutDesc&                 Layout,
                                                              std::vector<LayoutElement>&      Elements)
{
    Elements.clear();
    for (Uint32 input_elem = 0; VertexElement[input_elem].Stream != 0xFF; ++input_elem)
    {
        const auto& SrcElem = VertexElement[input_elem];
        Int32 InputIndex = -1;
        switch(SrcElem.Usage)
        {
            case DXSDKMESH_VERTEX_SEMANTIC_POSITION:
                InputIndex = 0;
            break;

            case DXSDKMESH_VERTEX_SEMANTIC_NORMAL:
                InputIndex = 1;
            break;

            case DXSDKMESH_VERTEX_SEMANTIC_TEXCOORD:
                InputIndex = 2;
            break;
        }

        if (InputIndex >= 0)
        {
            Uint32     NumComponents = 0;
            VALUE_TYPE ValueType     = VT_UNDEFINED;
            Bool       IsNormalized  = False;
            switch(SrcElem.Type)
            {
                case DXSDKMESH_VERTEX_DATA_TYPE_FLOAT2:
                    NumComponents = 2;
                    ValueType     = VT_FLOAT32;
                    IsNormalized  = False;
                break;

                case DXSDKMESH_VERTEX_DATA_TYPE_FLOAT3:
                    NumComponents = 3;
                    ValueType     = VT_FLOAT32;
                    IsNormalized  = False;
                break;

                default:
                    UNEXPECTED("Unsupported data type. Please add appropriate case statement.");
            }
            Elements.emplace_back(InputIndex, SrcElem.Stream, NumComponents, ValueType, IsNormalized, SrcElem.Offset, Stride);
        }
    }
    Layout.LayoutElements = Elements.data();
    Layout.NumElements    = static_cast<Uint32>(Elements.size());
}

void ShadowsSample::CreatePipelineStates(IRenderDevice* pDevice)
{
    ShaderCreateInfo ShaderCI;
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = true;

    ShaderMacroHelper Macros;
    Macros.AddShaderMacro("SHADOW_MODE",             m_ShadowSetting.iShadowMode);
    Macros.AddShaderMacro( "SHADOW_FILTER_SIZE",     m_LightAttribs.ShadowAttribs.iFixedFilterSize);
    Macros.AddShaderMacro( "FILTER_ACROSS_CASCADES", m_ShadowSetting.FilterAcrossCascades);
    Macros.AddShaderMacro( "BEST_CASCADE_SEARCH",    m_ShadowSetting.SearchBestCascade );
    ShaderCI.Macros = Macros;

    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.Desc.Name       = "Mesh VS";
    ShaderCI.EntryPoint      = "MeshVS";
    ShaderCI.FilePath        = "MeshVS.vsh";
    RefCntAutoPtr<IShader> pVS;
    m_pDevice->CreateShader(ShaderCI, &pVS);

    ShaderCI.Desc.Name   = "Mesh PS";
    ShaderCI.EntryPoint  = "MeshPS";
    ShaderCI.FilePath    = "MeshPS.psh";
    ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
    RefCntAutoPtr<IShader> pPS;
    m_pDevice->CreateShader(ShaderCI, &pPS);

    Macros.AddShaderMacro("SHADOW_PASS", true);
    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.Desc.Name       = "Mesh VS";
    ShaderCI.EntryPoint      = "MeshVS";
    ShaderCI.FilePath        = "MeshVS.vsh";
    ShaderCI.Macros          = Macros;
    RefCntAutoPtr<IShader> pShadowVS;
    m_pDevice->CreateShader(ShaderCI, &pShadowVS);

    m_PSOIndex.resize(m_Mesh.GetNumVBs());
    m_RenderMeshPSO.clear();
    m_RenderMeshShadowPSO.clear();
    for(Uint32 vb = 0; vb < m_Mesh.GetNumVBs(); ++vb)
    {
        PipelineStateDesc PSODesc;
        std::vector<LayoutElement> Elements;
        auto& InputLayout = PSODesc.GraphicsPipeline.InputLayout;
        DXSDKMESH_VERTEX_ELEMENTtoInputLayoutDesc(m_Mesh.VBElements(vb), m_Mesh.GetVertexStride(vb), InputLayout, Elements);
        
        //  Try to find PSO with the same layout
        Uint32 pso;
        for (pso=0; pso < m_RenderMeshPSO.size(); ++pso)
        {
            const auto& PSOLayout = m_RenderMeshPSO[pso]->GetDesc().GraphicsPipeline.InputLayout;
            bool IsSameLayout =
                PSOLayout.NumElements == InputLayout.NumElements && 
                memcmp(PSOLayout.LayoutElements, InputLayout.LayoutElements, sizeof(LayoutElement) * InputLayout.NumElements) == 0;
            
            if (IsSameLayout)
                break;
        }
        
        m_PSOIndex[vb] = pso;
        if (pso < static_cast<Uint32>(m_RenderMeshPSO.size()))
            continue;

        StaticSamplerDesc StaticSamplers[] =
        {
            {SHADER_TYPE_PIXEL, "g_tex2DDiffuse", Sam_Aniso4xWrap}
        };
        PSODesc.ResourceLayout.StaticSamplers    = StaticSamplers;
        PSODesc.ResourceLayout.NumStaticSamplers = _countof(StaticSamplers);

        ShaderResourceVariableDesc Vars[] = 
        {
            {SHADER_TYPE_PIXEL, "g_tex2DDiffuse",   SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL, m_ShadowSetting.iShadowMode == SHADOW_MODE_PCF ? "g_tex2DShadowMap" : "g_tex2DFilterableShadowMap",   SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };
        PSODesc.ResourceLayout.Variables    = Vars;
        PSODesc.ResourceLayout.NumVariables = _countof(Vars);

        PSODesc.Name = "Mesh PSO";
        PSODesc.GraphicsPipeline.pVS = pVS;
        PSODesc.GraphicsPipeline.pPS = pPS;

        PSODesc.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
        PSODesc.GraphicsPipeline.NumRenderTargets = 1;
        PSODesc.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
        PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS_EQUAL;

        RefCntAutoPtr<IPipelineState> pRenderMeshPSO;
        m_pDevice->CreatePipelineState(PSODesc, &pRenderMeshPSO);
        pRenderMeshPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbCameraAttribs")->Set(m_CameraAttribsCB);
        pRenderMeshPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL,  "cbLightAttribs")->Set(m_LightAttribsCB);
        pRenderMeshPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbLightAttribs")->Set(m_LightAttribsCB);

        PSODesc.Name = "Mesh Shadow PSO";
        PSODesc.GraphicsPipeline.pPS        = nullptr;
        PSODesc.GraphicsPipeline.pVS        = pShadowVS;
        PSODesc.GraphicsPipeline.NumRenderTargets = 0;
        PSODesc.GraphicsPipeline.RTVFormats[0]    = TEX_FORMAT_UNKNOWN;
        PSODesc.GraphicsPipeline.DSVFormat        = m_ShadowSetting.Format;

        // It is crucial to disable depth clip to allow shadows from objects
        // behind the near cascade clip plane!
        PSODesc.GraphicsPipeline.RasterizerDesc.DepthClipEnable = False;

        PSODesc.ResourceLayout.StaticSamplers     = nullptr;
        PSODesc.ResourceLayout.NumStaticSamplers  = 0;
        PSODesc.ResourceLayout.Variables          = nullptr;
        PSODesc.ResourceLayout.NumVariables       = 0;
        RefCntAutoPtr<IPipelineState> pRenderMeshShadowPSO;
        m_pDevice->CreatePipelineState(PSODesc, &pRenderMeshShadowPSO);
        pRenderMeshShadowPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbCameraAttribs")->Set(m_CameraAttribsCB);

        m_RenderMeshPSO.emplace_back(std::move(pRenderMeshPSO));
        m_RenderMeshShadowPSO.emplace_back(std::move(pRenderMeshShadowPSO));
    }
}

void ShadowsSample::InitializeResourceBindings()
{
    m_SRBs.clear();
    m_ShadowSRBs.clear();
    m_SRBs.resize(m_Mesh.GetNumMaterials());
    m_ShadowSRBs.resize(m_Mesh.GetNumMaterials());
    for(Uint32 mat = 0; mat < m_Mesh.GetNumMaterials(); ++mat)
    {
        {
            const auto& Mat = m_Mesh.GetMaterial(mat);
            RefCntAutoPtr<IShaderResourceBinding> pSRB;
            m_RenderMeshPSO[0]->CreateShaderResourceBinding(&pSRB, true);
            VERIFY(Mat.pDiffuseRV != nullptr, "Material must have diffuse color texture");
            pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_tex2DDiffuse")->Set(Mat.pDiffuseRV);
            if (m_ShadowSetting.iShadowMode == SHADOW_MODE_PCF)
            {
                pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_tex2DShadowMap")->Set(m_ShadowMapMgr.GetSRV());
            }
            else
            {
                pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_tex2DFilterableShadowMap")->Set(m_ShadowMapMgr.GetSRV());
            }
            m_SRBs[mat] = std::move(pSRB);
        }

        {
            RefCntAutoPtr<IShaderResourceBinding> pShadowSRB;
            m_RenderMeshShadowPSO[0]->CreateShaderResourceBinding(&pShadowSRB, true);
            m_ShadowSRBs[mat] = std::move(pShadowSRB);
        }
    }
}

void ShadowsSample::CreateShadowMap()
{
    m_LightAttribs.ShadowAttribs.fNumCascades = static_cast<float>(m_LightAttribs.ShadowAttribs.iNumCascades);
    ShadowMapManager::InitInfo SMMgrInitInfo;
    SMMgrInitInfo.Fmt         = m_ShadowSetting.Format;
    SMMgrInitInfo.Resolution  = m_ShadowSetting.Resolution;
    SMMgrInitInfo.NumCascades = static_cast<Uint32>(m_LightAttribs.ShadowAttribs.iNumCascades);
    SMMgrInitInfo.ShadowMode  = m_ShadowSetting.iShadowMode;
    SMMgrInitInfo.Is32BitFilterableFmt = m_ShadowSetting.Is32BitFilterableFmt;

    if (!m_pComparisonSampler)
    {
        SamplerDesc ComparsionSampler;
        ComparsionSampler.ComparisonFunc = COMPARISON_FUNC_LESS; 
        // Note: anisotropic filtering requires SampleGrad to fix artifacts at 
        // cascade boundaries
        ComparsionSampler.MinFilter      = FILTER_TYPE_COMPARISON_LINEAR;
        ComparsionSampler.MagFilter      = FILTER_TYPE_COMPARISON_LINEAR;
        ComparsionSampler.MipFilter      = FILTER_TYPE_COMPARISON_LINEAR;
        m_pDevice->CreateSampler(ComparsionSampler, &m_pComparisonSampler);
    }
    SMMgrInitInfo.pComparisonSampler = m_pComparisonSampler;

    if (!m_pFilterableShadowMapSampler)
    {
        SamplerDesc SamplerDesc;
        SamplerDesc.MinFilter     = FILTER_TYPE_ANISOTROPIC;
        SamplerDesc.MagFilter     = FILTER_TYPE_ANISOTROPIC;
        SamplerDesc.MipFilter     = FILTER_TYPE_ANISOTROPIC;
        SamplerDesc.MaxAnisotropy = m_LightAttribs.ShadowAttribs.iMaxAnisotropy;
        m_pDevice->CreateSampler(SamplerDesc, &m_pFilterableShadowMapSampler);
    }
    SMMgrInitInfo.pFilterableShadowMapSampler = m_pFilterableShadowMapSampler;

    m_ShadowMapMgr.Initialize(m_pDevice, SMMgrInitInfo);

    InitializeResourceBindings();
}

void ShadowsSample::RenderShadowMap()
{
    auto iNumShadowCascades = m_LightAttribs.ShadowAttribs.iNumCascades;
    for(int iCascade = 0; iCascade < iNumShadowCascades; ++iCascade)
    {
        const auto CascadeProjMatr = m_ShadowMapMgr.GetCascadeTranform(iCascade).Proj;

        auto WorldToLightViewSpaceMatr = m_LightAttribs.ShadowAttribs.mWorldToLightViewT.Transpose();
        auto WorldToLightProjSpaceMatr = WorldToLightViewSpaceMatr * CascadeProjMatr;
        CameraAttribs ShadowCameraAttribs = {};
        ShadowCameraAttribs.mViewT     = m_LightAttribs.ShadowAttribs.mWorldToLightViewT;
        ShadowCameraAttribs.mProjT     = CascadeProjMatr.Transpose();
        ShadowCameraAttribs.mViewProjT = WorldToLightProjSpaceMatr.Transpose();
        ShadowCameraAttribs.f4ViewportSize.x = static_cast<float>(m_ShadowSetting.Resolution);
        ShadowCameraAttribs.f4ViewportSize.y = static_cast<float>(m_ShadowSetting.Resolution);
        ShadowCameraAttribs.f4ViewportSize.z = 1.f / ShadowCameraAttribs.f4ViewportSize.x;
        ShadowCameraAttribs.f4ViewportSize.w = 1.f / ShadowCameraAttribs.f4ViewportSize.y;

        {
            MapHelper<CameraAttribs> CameraData(m_pImmediateContext, m_CameraAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
            *CameraData = ShadowCameraAttribs;
        }

        auto* pCascadeDSV = m_ShadowMapMgr.GetCascadeDSV(iCascade);
        m_pImmediateContext->SetRenderTargets(0, nullptr, pCascadeDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pCascadeDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawMesh(m_pImmediateContext, true);
    }

    if (m_ShadowSetting.iShadowMode > SHADOW_MODE_PCF)
        m_ShadowMapMgr.ConvertToFilterable(m_pImmediateContext, m_LightAttribs.ShadowAttribs);
}

// Render a frame
void ShadowsSample::Render()
{
    RenderShadowMap();

    // Reset default framebuffer
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    // Clear the back buffer 
    const float ClearColor[] = { 0.032f,  0.032f,  0.032f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        MapHelper<LightAttribs> LightData(m_pImmediateContext, m_LightAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
        *LightData = m_LightAttribs;
    }

    {
        const auto& CameraView  = m_Camera.GetViewMatrix();
        const auto& CameraWorld = m_Camera.GetWorldMatrix();
        float3 CameraWorldPos = float3::MakeVector(CameraWorld[3]);
        const auto& Proj = m_Camera.GetProjMatrix();
        auto CameraViewProj = CameraView * Proj;

        MapHelper<CameraAttribs> CamAttribs(m_pImmediateContext, m_CameraAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
        CamAttribs->mProjT        = Proj.Transpose();
        CamAttribs->mViewProjT    = CameraViewProj.Transpose();
        CamAttribs->mViewProjInvT = CameraViewProj.Inverse().Transpose();
        CamAttribs->f4Position = float4(CameraWorldPos, 1);
    }

    // Note that Vulkan requires shadow map to be transitioned to DEPTH_READ state, not SHADER_RESOURCE
    m_pImmediateContext->TransitionShaderResources(m_RenderMeshPSO[0], m_SRBs[0]);

    DrawMesh(m_pImmediateContext, false);
}


void ShadowsSample::DrawMesh(IDeviceContext* pCtx, bool bIsShadowPass)
{
    //uint32 partCount = 0;
    for (Uint32 meshIdx = 0; meshIdx < m_Mesh.GetNumMeshes(); ++meshIdx)
    {
        const auto& SubMesh = m_Mesh.GetMesh(meshIdx);

        IBuffer* pVBs[]  = {m_Mesh.GetMeshVertexBuffer(meshIdx, 0)};
        Uint32 Offsets[] = {0};
        pCtx->SetVertexBuffers(0, 1, pVBs, Offsets, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);

        auto* pIB = m_Mesh.GetMeshIndexBuffer(meshIdx);
        auto IBFormat = m_Mesh.GetIBFormat(meshIdx);
        
        pCtx->SetIndexBuffer(pIB, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

        auto PSOIndex = m_PSOIndex[SubMesh.VertexBuffers[0]];
        auto& pPSO = (bIsShadowPass ? m_RenderMeshShadowPSO : m_RenderMeshPSO)[PSOIndex];
        pCtx->SetPipelineState(pPSO);

        // Draw all subsets
        for(Uint32 subsetIdx = 0; subsetIdx < SubMesh.NumSubsets; ++subsetIdx)
        {
            //if(meshData.FrustumTests[partCount++])
            {
                const auto& Subset = m_Mesh.GetSubset(meshIdx, subsetIdx);
                pCtx->CommitShaderResources((bIsShadowPass ? m_ShadowSRBs : m_SRBs)[Subset.MaterialID], RESOURCE_STATE_TRANSITION_MODE_VERIFY);

                DrawAttribs drawAttrs(static_cast<Uint32>(Subset.IndexCount), IBFormat, DRAW_FLAG_VERIFY_ALL);
                drawAttrs.FirstIndexLocation = static_cast<Uint32>(Subset.IndexStart);
                pCtx->Draw(drawAttrs);
            }
        }
    }
}

void ShadowsSample::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));
    {
        const auto& mouseState = m_InputController.GetMouseState();
        if (m_LastMouseState.PosX >= 0 &&
            m_LastMouseState.PosY >= 0 &&
           (m_LastMouseState.ButtonFlags & MouseState::BUTTON_FLAG_RIGHT) != 0)
        {
            constexpr float LightRotationSpeed = 0.001f;
            float fYawDelta   = (mouseState.PosX - m_LastMouseState.PosX) * LightRotationSpeed;
            float fPitchDelta = (mouseState.PosY - m_LastMouseState.PosY) * LightRotationSpeed;
            float3& f3LightDir = reinterpret_cast<float3&>(m_LightAttribs.f4Direction);
            f3LightDir = float4(f3LightDir, 0) *
                           float4x4::RotationArbitrary(m_Camera.GetWorldUp(),    fYawDelta) * 
                           float4x4::RotationArbitrary(m_Camera.GetWorldRight(), fPitchDelta);
        }

        m_LastMouseState = mouseState;
    }


    m_LightAttribs.ShadowAttribs.fCascadePartitioningFactor = 1.f;
    ShadowMapManager::DistributeCascadeInfo DistrInfo;
    DistrInfo.pCameraView = &m_Camera.GetViewMatrix();
    DistrInfo.pCameraProj = &m_Camera.GetProjMatrix();
    float3 CameraPos = m_Camera.GetPos();
    DistrInfo.pCameraPos  = &CameraPos;
    float3 f3LightDirection = float3(m_LightAttribs.f4Direction.x, m_LightAttribs.f4Direction.y, m_LightAttribs.f4Direction.z);
    DistrInfo.pLightDir   = &f3LightDirection;

    DistrInfo.SnapCascades     = m_ShadowSetting.SnapCascades;
    DistrInfo.EqualizeExtents  = m_ShadowSetting.EqualizeExtents;
    DistrInfo.StabilizeExtents = m_ShadowSetting.StabilizeExtents;

    m_ShadowMapMgr.DistributeCascades(DistrInfo, m_LightAttribs.ShadowAttribs);
}

void ShadowsSample::WindowResize(Uint32 Width, Uint32 Height)
{
    float NearPlane = 0.1f;
    float FarPlane = 250.f;
    float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 4.f, m_pDevice->GetDeviceCaps().IsGLDevice());
}

}
