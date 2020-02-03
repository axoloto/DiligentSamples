/*
 *  Copyright 2019-2020 Diligent Graphics LLC
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

#include <cmath>
#include <algorithm>
#include <array>

#include "TerrainRenderer.h"
#include "MapHelper.h"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "imgui.h"
#include "imGuIZMO.h"
#include "PlatformMisc.h"
#include "ImGuiUtils.h"

namespace Diligent
{
    static float g_time = 0.0;

SampleBase* CreateSample()
{
    return new TerrainRenderer();
}

void TerrainRenderer::Initialize(IEngineFactory* pEngineFactory, IRenderDevice* pDevice, IDeviceContext** ppContexts, Uint32 NumDeferredCtx, ISwapChain* pSwapChain)
{
    const auto& deviceCaps = pDevice->GetDeviceCaps();
    if (!deviceCaps.Features.ComputeShaders)
    {
        throw std::runtime_error("Compute shaders are required to run this sample");
    }

    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    m_earthMesh.Create(m_pDevice,
                       m_pImmediateContext,
                       m_iNbGridSidePts,
                       m_fLightPosition);
}

void TerrainRenderer::UpdateUI()
{
    bool newHeightScale = false, newGridResolution = false, newLightDirection = false;
    bool newXLightPos = false, newYLightPos = false, newZLightPos = false;
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Checkbox("Animate", &m_bAnimateGrid);

        newXLightPos = ImGui::SliderFloat("X Light Position", &m_fLightPosition[0], -1.f, 1.f);
        newYLightPos = ImGui::SliderFloat("Y Light Position", &m_fLightPosition[1], -1.f, 1.f);
        newZLightPos = ImGui::SliderFloat("Z Light Position", &m_fLightPosition[2], 0.f, 2.f);

        newGridResolution = ImGui::SliderInt("Grid Resolution", &m_iNbGridSidePts, 2, 600);
        newHeightScale    = ImGui::SliderFloat("Height Scale", &m_fElevationScale, 1.f, 100.f);
    }
    ImGui::End();

    // Updating value on the grid side
    if (newXLightPos || newYLightPos || newZLightPos)
    {
        m_earthMesh.UpdateLightPosition(m_fLightPosition);
    }
    if (newGridResolution)
    {
        m_earthMesh.UpdateGridResolution(m_iNbGridSidePts);
    }
    if (newHeightScale)
    {
        m_earthMesh.SetHeightScale(m_fElevationScale / 500.0f);
    }
}

TerrainRenderer::~TerrainRenderer()
{
}

// Render a frame
void TerrainRenderer::Render()
{
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    // Clear the back buffer
    const float ClearColor[] = { 0.350f, 0.350f, 0.350f, 1.0f };
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Render terrain
    m_earthMesh.Render();
}

void TerrainRenderer::Update(double CurrTime, double ElapsedTime)
{
    const auto& mouseState = m_InputController.GetMouseState();

    m_fZCamPos -= mouseState.WheelDelta * 0.25f;
    m_fZCamPos = std::max(m_fZCamPos, 0.f);
    m_fZCamPos = std::min(m_fZCamPos, 10.f);

    SampleBase::Update(CurrTime, ElapsedTime);
    
    UpdateUI();

    const bool isGL = m_pDevice->GetDeviceCaps().IsGLDevice();
    g_time += (m_bAnimateGrid ? static_cast<float>(ElapsedTime) : 0.0f);

    // Camera transform
    float4x4 viewWorldTrans = float4x4::Translation(-0.5f, -0.5f, 0.0f)
                            * float4x4::RotationZ(g_time * 0.4f)
                            * float4x4::RotationX(3.4f*PI_F / 2.5f * 1.0f)
                            * float4x4::Translation(0.0f, 0.0f, m_fZCamPos);      

    // Used for lighting model
    m_earthMesh.SetViewWorldMatrix(viewWorldTrans);   
    m_earthMesh.UpdateLightPosition(m_fLightPosition);

    // Projection transform
    float nearPlane = 0.1f;
    float farPlane = 100.f;
    float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
    // Projection matrix differs between DX and OpenGL
    auto projTrans = float4x4::Projection(PI_F / 4.f, aspectRatio, nearPlane, farPlane, isGL);

    // Full grid transform
    float4x4 gridProjViewWorld = viewWorldTrans
                               * projTrans;

    m_earthMesh.SetWorldViewProjMatrix(gridProjViewWorld);

    // Full light cube transform
    float4x4 lightProjViewWorld = float4x4::Translation(m_fLightPosition[0], m_fLightPosition[1], m_fLightPosition[2])
                                * viewWorldTrans
                                * projTrans;
    
    m_earthMesh.SetWorldViewProjLightMatrix(lightProjViewWorld);
}

} // namespace Diligent
