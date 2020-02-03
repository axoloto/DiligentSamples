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

#pragma once

#include "SampleBase.h"
#include "BasicMath.h"
#include "EarthMesh.h"

namespace Diligent
{

class TerrainRenderer final : public SampleBase
{
public:
    TerrainRenderer() : m_fElapsedTime(0.0f),
                      m_fLightPosition(normalize(float3(0.01f, 0.01f, 0.01f))),
                      m_bAnimateGrid(true),
                      m_fElevationScale(90.0f),
                      m_fZCamPos(3.0f),
                      m_iNbGridSidePts(100){}
    ~TerrainRenderer();

    virtual void        Initialize(IEngineFactory*  pEngineFactory,
                                   IRenderDevice*   pDevice,
                                   IDeviceContext** ppContexts,
                                   Uint32           NumDeferredCtx,
                                   ISwapChain*      pSwapChain) override final;
    virtual void        Render() override final;
    virtual void        Update(double CurrTime, double ElapsedTime) override final;
    virtual const Char* GetSampleName() const override final { return "TerrainRenderer"; }
    const uint          GetNumGridSidePts() const { return m_iNbGridSidePts; }

private:
    void UpdateUI();

    EarthMesh m_earthMesh;

    float     m_fElapsedTime;
    float3    m_f3CustomRlghBeta, m_f3CustomMieBeta;

    RefCntAutoPtr<ITexture> m_pOffscreenColorBuffer;
    RefCntAutoPtr<ITexture> m_pOffscreenDepthBuffer;

    //UI
    float3 m_fLightPosition;
    bool   m_bAnimateGrid;
    float  m_fElevationScale;
    float  m_fZCamPos;
    int    m_iNbGridSidePts;
};

} // namespace Diligent
