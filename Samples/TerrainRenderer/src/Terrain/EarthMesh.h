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

// This file is derived from the open source project provided by Intel Corportaion that
// requires the following notice to be kept:
//--------------------------------------------------------------------------------------
// Copyright 2013 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------

#pragma once

#include <vector>

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "Buffer.h"
#include "Texture.h"
#include "BufferView.h"
#include "TextureView.h"
#include "GraphicsTypes.h"
#include "TextureUtilities.h"
#include "RefCntAutoPtr.h"

#include "AdvancedMath.h"

namespace Diligent
{

class EarthMesh
{
public:
    EarthMesh(void){}

    EarthMesh             (const EarthMesh&) = delete;
    EarthMesh& operator = (const EarthMesh&) = delete;
    EarthMesh             (EarthMesh&&)      = delete;
    EarthMesh& operator = (EarthMesh&&)      = delete;

    // Renders the model
    void Render();
    void RenderEarth();
    void RenderLight();

    // Creates device resources
    void Create(IRenderDevice*  pDevice,
                IDeviceContext* pContext,
                uint            nbGridSidePts,
                float3          fLightDirection);

    void SetWorldViewProjMatrix(float4x4 worldViewProjMatrix) { m_fMatWorldViewProj = worldViewProjMatrix; }
    void SetViewWorldMatrix(float4x4 worldViewMatrix) { m_fMatWorldView = worldViewMatrix; }
    void SetWorldViewProjLightMatrix(float4x4 worldViewProjLightMatrix) { m_fMatWorldViewProjLight = worldViewProjLightMatrix; }

    void SetHeightScale(float heightScale)  { m_fHeightScale = heightScale; }
    void UpdateGridResolution(int newGridResolution);
    void UpdateLightPosition(float3 newLightPosition);
private:
    void CreateEarthPipeline();
    void CreateLightPipeline();
    void CreateEarthGrid();
    void CreateLightCube();
    void LoadEarthTexture();

    RefCntAutoPtr<IRenderDevice>          m_pDevice;
    RefCntAutoPtr<IDeviceContext>         m_pContext;

    RefCntAutoPtr<IPipelineState>         m_pEarthPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pEarthSRB;

    RefCntAutoPtr<IPipelineState>         m_pLightPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pLightSRB;

    RefCntAutoPtr<IBuffer>                m_pGridVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_pGridIndexBuffer;

    RefCntAutoPtr<IBuffer>                m_pLightCubeVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_pLightCubeIndexBuffer;

    RefCntAutoPtr<IBuffer>                m_pVSConstants;
    RefCntAutoPtr<ITextureView>           m_textureSRV;
    RefCntAutoPtr<ITextureView>           m_heightMapSRV;

    float4x4                              m_fMatWorldViewProj;
    float4x4                              m_fMatWorldViewProjLight;
    float4x4                              m_fMatWorldView;

    float                                 m_fHeightScale;
    float3                                m_fLightPosition;

    struct Vertex
    {
        //float2 pos;
        float2 uv;
    };

    std::vector<Vertex>                   m_gridVertices;
    std::vector<uint>                     m_triConnec;
    uint                                  m_iNbGridSidePts;
};

} // namespace Diligent
