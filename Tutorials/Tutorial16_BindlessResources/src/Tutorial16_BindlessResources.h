/*     Copyright 2019 Diligent Graphics LLC
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

#pragma once 

#include <vector>
#include "SampleBase.h"
#include "BasicMath.h"

namespace Diligent
{

class Tutorial16_BindlessResources final: public SampleBase
{
public:
    virtual void Initialize(IEngineFactory*  pEngineFactory,
                            IRenderDevice*   pDevice, 
                            IDeviceContext** ppContexts, 
                            Uint32           NumDeferredCtx, 
                            ISwapChain*      pSwapChain)override final;
    virtual void Render()override final;
    virtual void Update(double CurrTime, double ElapsedTime)override final;
    virtual const Char* GetSampleName()const override final{return "Tutorial16: Bindless Resources";}

    struct ObjectGeometry
    {
        Uint32 FirstIndex = 0;
        Uint32 NumIndices = 0;
        Uint32 BaseVertex = 0;
    };

private:
    void CreatePipelineState();
    void CreateGeometryBuffers();
    void CreateInstanceBuffer();
    void LoadTextures();
    void UpdateUI();
    void PopulateInstanceBuffer();

    static constexpr int    NumTextures    = 4;
    std::vector<ObjectGeometry> m_Geometries;

    bool                                  m_BindlessMode = false;

    RefCntAutoPtr<IPipelineState>         m_pPSO;
    RefCntAutoPtr<IPipelineState>         m_pBindlessPSO;
    RefCntAutoPtr<IBuffer>                m_VertexBuffer;
    RefCntAutoPtr<IBuffer>                m_IndexBuffer;
    RefCntAutoPtr<IBuffer>                m_InstanceBuffer;
    RefCntAutoPtr<IBuffer>                m_VSConstants;
    RefCntAutoPtr<IShaderResourceBinding> m_SRB[NumTextures];
    RefCntAutoPtr<IShaderResourceBinding> m_BindlessSRB;
    
    struct InstanceData
    {
        float4x4 Matrix;
        uint     TextureInd;
    };
    std::vector<InstanceData> m_InstanceData;
    std::vector<Uint32>       m_GeometryType;

    float4x4 m_ViewProjMatrix;
    float4x4 m_RotationMatrix;
    int m_GridSize = 5;
    static constexpr int MaxGridSize  = 32;
    static constexpr int MaxInstances = MaxGridSize * MaxGridSize * MaxGridSize;
    
};

}