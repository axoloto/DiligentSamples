
#include "EarthMesh.h"
#include "MapHelper.h"
#include "GraphicsUtilities.h"
#include <array>


namespace Diligent {

    struct GlobalConstants
    {
        float4x4 g_worldViewProj;
        float4x4 g_worldView;
        float3   g_lightPosition;
        float    g_heightScale;
        float    g_gridOffset;
    };

    void EarthMesh::Create(IRenderDevice*  pDevice,
                           IDeviceContext* pContext,
                           uint            nbGridSidePts,
                           float3          lightPosition){

        m_pDevice = pDevice;
        m_pContext = pContext;
        m_iNbGridSidePts = nbGridSidePts;
        m_fLightPosition = lightPosition;
        m_fHeightScale = 0.1f;

        // Create dynamic uniform buffer that will store our transformation matrix
        // Dynamic buffers can be frequently updated by the CPU
        CreateUniformBuffer(m_pDevice, sizeof(GlobalConstants), "VS constants CB", &m_pVSConstants);

        CreateEarthPipeline();
        CreateLightPipeline();

        CreateEarthGrid();
        LoadEarthTexture();

        CreateLightCube();
    }

    void EarthMesh::UpdateLightPosition(float3 newLightPosition)
    {
        m_fLightPosition = newLightPosition;
    }

    void EarthMesh::UpdateGridResolution(int newGridResolution)
    {
        m_iNbGridSidePts = newGridResolution;
        // Update Grid
        CreateEarthGrid();
    }

    void EarthMesh::Render()
    {
        RenderEarth();
        RenderLight();
    }

    void EarthMesh::RenderEarth()
    {
        {
            // Map the buffer and write current world-view-projection matrix
            MapHelper<GlobalConstants> CBConstants(m_pContext, m_pVSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants->g_worldViewProj  = m_fMatWorldViewProj.Transpose();
            CBConstants->g_worldView      = m_fMatWorldView.Transpose();
            CBConstants->g_lightPosition  = m_fLightPosition;
            CBConstants->g_heightScale    = m_fHeightScale;
            CBConstants->g_gridOffset     = (float)(1/(m_iNbGridSidePts-1));
        }

        // Bind vertex and index buffers
        Uint32   offset = 0;
        IBuffer* pBuffs[] = { m_pGridVertexBuffer };
        m_pContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pContext->SetIndexBuffer(m_pGridIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Set the pipeline state
        m_pContext->SetPipelineState(m_pEarthPSO);
        // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
        // makes sure that resources are transitioned to required states.
        m_pContext->CommitShaderResources(m_pEarthSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
        DrawAttrs.IndexType = VT_UINT32; // Index type
        DrawAttrs.NumIndices = (m_iNbGridSidePts -1)*(m_iNbGridSidePts -1)*6;
        // Verify the state of vertex and index buffers
        DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
        m_pContext->DrawIndexed(DrawAttrs);
    }
    
    void EarthMesh::RenderLight()
    {
        {
            // Map the buffer and write current world-view-projection matrix
            MapHelper<GlobalConstants> CBConstants(m_pContext, m_pVSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants->g_worldViewProj = m_fMatWorldViewProjLight.Transpose();
        }

        m_pContext->SetPipelineState(m_pLightPSO);

        // Bind vertex and index buffers
        Uint32   offset = 0;
        IBuffer* pBuffs[] = { m_pLightCubeVertexBuffer };
        m_pContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pContext->SetIndexBuffer(m_pLightCubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        
        // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
        // makes sure that resources are transitioned to required states.
        m_pContext->CommitShaderResources(m_pLightSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs DrawAttrs;    // This is an indexed draw call
        DrawAttrs.IndexType = VT_UINT32; // Index type
        DrawAttrs.NumIndices = 36;
        // Verify the state of vertex and index buffers
        DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
        m_pContext->DrawIndexed(DrawAttrs);
    }
    
    void EarthMesh::CreateEarthPipeline()
    {
        // Pipeline state object encompasses configuration of all GPU stages

        PipelineStateDesc PSODesc;
        // Pipeline state name is used by the engine to report issues.
        // It is always a good idea to give objects descriptive names.
        PSODesc.Name = "EarthMesh PSO";

        // This is a graphics pipeline
        PSODesc.IsComputePipeline = false;

        // clang-format off
        // This tutorial will render to a single render target
        PSODesc.GraphicsPipeline.NumRenderTargets = 1;
        // Set render target format which is the format of the swap chain's color buffer
        PSODesc.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM_SRGB;
        // Set depth buffer format which is the format of the swap chain's back buffer
        PSODesc.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
        // Primitive topology defines what kind of primitives will be rendered by this pipeline state
        PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // Cull back faces
        PSODesc.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_FRONT;
        // Enable depth testing
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

        ShaderCreateInfo ShaderCI;
        // Tell the system that the shader source code is in HLSL.
        // For OpenGL, the engine will convert this into GLSL under the hood.
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

        // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
        ShaderCI.UseCombinedTextureSamplers = true;

        // We will load shaders from file. To be able to do that,
        // we need to create a shader source stream factory
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders\\", &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        // Create a vertex shader
        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Earth VS";
            ShaderCI.FilePath = "earth.vsh";
            m_pDevice->CreateShader(ShaderCI, &pVS);
        }

        // Create a pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Earth PS";
            ShaderCI.FilePath = "earth.psh";
            m_pDevice->CreateShader(ShaderCI, &pPS);
        }

        // Define vertex shader input layout
        LayoutElement LayoutElems[] =
        {
            // Attribute 0 - uv used for spatial coords and texture coords
            LayoutElement{ 0, 0, 2, VT_FLOAT32, False },
        };

        PSODesc.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        PSODesc.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

        PSODesc.GraphicsPipeline.pVS = pVS;
        PSODesc.GraphicsPipeline.pPS = pPS;

        // Define variable type that will be used by default
        PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        // Shader variables should typically be mutable, which means they are expected
        // to change on a per-instance basis
        ShaderResourceVariableDesc Vars[] =
        {
            { SHADER_TYPE_VERTEX, "g_heightMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE },
            { SHADER_TYPE_PIXEL,  "g_texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE }
        };
        PSODesc.ResourceLayout.Variables = Vars;
        PSODesc.ResourceLayout.NumVariables = _countof(Vars);

        // Define static sampler for g_Texture. Static samplers should be used whenever possible
        SamplerDesc SamLinearClampDesc
        {
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
            TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
        };
        StaticSamplerDesc StaticSamplers[] =
        {
            { SHADER_TYPE_VERTEX, "g_heightMap", SamLinearClampDesc },
            { SHADER_TYPE_PIXEL,  "g_texture", SamLinearClampDesc }
        };
        // clang-format on
        PSODesc.ResourceLayout.StaticSamplers = StaticSamplers;
        PSODesc.ResourceLayout.NumStaticSamplers = _countof(StaticSamplers);

        m_pDevice->CreatePipelineState(PSODesc, &m_pEarthPSO);

        // Since we did not explicitly specify the type for 'Constants' variable, default
        // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
        // change and are bound directly through the pipeline state object.
        m_pEarthPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")->Set(m_pVSConstants);

        // Create a shader resource binding object and bind all static resources in it
        m_pEarthPSO->CreateShaderResourceBinding(&m_pEarthSRB, true);
    }

    void EarthMesh::CreateLightPipeline()
    {
        PipelineStateDesc PSODesc;
        // Pipeline state name is used by the engine to report issues.
        // It is always a good idea to give objects descriptive names.
        PSODesc.Name = "Light PSO";

        // This is a graphics pipeline
        PSODesc.IsComputePipeline = false;

        // This tutorial will render to a single render target
        PSODesc.GraphicsPipeline.NumRenderTargets = 1;
        // Set render target format which is the format of the swap chain's color buffer
        PSODesc.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM_SRGB;
        // Set depth buffer format which is the format of the swap chain's back buffer
        PSODesc.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
        // Primitive topology defines what kind of primitives will be rendered by this pipeline state
        PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // Cull back faces
        PSODesc.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
        // Enable depth testing
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

        ShaderCreateInfo ShaderCI;
        // Tell the system that the shader source code is in HLSL.
        // For OpenGL, the engine will convert this into GLSL under the hood.
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

        // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
        ShaderCI.UseCombinedTextureSamplers = true;

        // We will load shaders from file. To be able to do that,
        // we need to create a shader source stream factory
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders\\", &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
        // Create a vertex shader
        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Light VS";
            ShaderCI.FilePath = "light.vsh";
            m_pDevice->CreateShader(ShaderCI, &pVS);
        }

        // Create a pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint = "main";
            ShaderCI.Desc.Name = "Light PS";
            ShaderCI.FilePath = "light.psh";
            m_pDevice->CreateShader(ShaderCI, &pPS);
        }

        // Define vertex shader input layout
        LayoutElement LayoutElems[] =
        {
            // Attribute 0 - coords
            LayoutElement{ 0, 0, 3, VT_FLOAT32, False },
            // Attribute 1 - color
            LayoutElement{ 1, 0, 4, VT_FLOAT32, False },
        };
        // clang-format on
        PSODesc.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        PSODesc.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

        PSODesc.GraphicsPipeline.pVS = pVS;
        PSODesc.GraphicsPipeline.pPS = pPS;

        // Define variable type that will be used by default
        PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        m_pDevice->CreatePipelineState(PSODesc, &m_pLightPSO);

        // Since we did not explicitly specify the type for 'Constants' variable, default
        // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
        // change and are bound directly through the pipeline state object.
        m_pLightPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")->Set(m_pVSConstants);

        // Create a shader resource binding object and bind all static resources in it
        m_pLightPSO->CreateShaderResourceBinding(&m_pLightSRB, true);
    }

    void EarthMesh::CreateEarthGrid()
    {
        if (m_gridVertices.size() > 0 && m_pGridVertexBuffer) {
            m_gridVertices.clear();
            m_pGridVertexBuffer.Release();
        }

        if (m_triConnec.size() > 0 && m_pGridIndexBuffer) {
            m_triConnec.clear();
            m_pGridIndexBuffer.Release();
        }

        m_gridVertices.reserve(m_iNbGridSidePts*m_iNbGridSidePts);
        uint index = 0;
        float xPos = 0.0, yPos = 0.0;
        float u = 0, v = 0;
        for (uint i = 0; i < m_iNbGridSidePts; ++i) {
            for (uint j = 0; j < m_iNbGridSidePts; ++j) {

                xPos = (float)(index % m_iNbGridSidePts);
                yPos = (float)((index - xPos) / m_iNbGridSidePts);

                u = xPos / (m_iNbGridSidePts -1);
                v = yPos / (m_iNbGridSidePts -1);

                Vertex newVertex;
                newVertex.uv = { u, v };
                m_gridVertices.push_back(newVertex);

                ++index;
            }
        }

        // Create a vertex buffer that stores cube vertices
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "Grid Vertex buffer";
        VertBuffDesc.Usage = USAGE_STATIC;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.uiSizeInBytes = (Uint32)(sizeof(m_gridVertices[0]) * m_gridVertices.size());
        BufferData VBData;
        VBData.pData = &m_gridVertices[0];
        VBData.DataSize = (Uint32)(sizeof(m_gridVertices[0]) *  m_gridVertices.size());
        m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_pGridVertexBuffer);

        const size_t nbTotTriIndices = (m_iNbGridSidePts -1) * (m_iNbGridSidePts -1) * 6;
        m_triConnec.reserve(nbTotTriIndices);

        uint nbDuoTrisDoneOnTheLine = 0;
        int lastPass = m_iNbGridSidePts*m_iNbGridSidePts - (m_iNbGridSidePts + 1);
        for (int i = 0, j = 0; i < lastPass; j +=6, ++i) {

            m_triConnec.push_back(i + m_iNbGridSidePts + 1);
            m_triConnec.push_back(i + 1);
            m_triConnec.push_back(i);

            m_triConnec.push_back(i + m_iNbGridSidePts + 1);
            m_triConnec.push_back(i) ;
            m_triConnec.push_back(i + m_iNbGridSidePts);

            ++nbDuoTrisDoneOnTheLine;
            // Jump to the next line
            if (nbDuoTrisDoneOnTheLine == m_iNbGridSidePts -1) {
                nbDuoTrisDoneOnTheLine = 0;
                ++i;
            }
        }

        BufferDesc IndBuffDesc;
        IndBuffDesc.Name = "Grid index buffer";
        IndBuffDesc.Usage = USAGE_STATIC;
        IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
        IndBuffDesc.uiSizeInBytes = (Uint32)(sizeof(m_triConnec[0]) * m_triConnec.size());
        BufferData IBData;
        IBData.pData = &m_triConnec[0];
        IBData.DataSize = (Uint32)(sizeof(m_triConnec[0]) * m_triConnec.size());
        m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_pGridIndexBuffer);
    }

    void EarthMesh::CreateLightCube()
    {
        // Layout of this structure matches the one we defined in the pipeline state
        struct Vertex
        {
            float3 pos;
            float4 color;
        };

        // Cube vertices

        //      (-1,+1,+1)________________(+1,+1,+1)
        //               /|              /|
        //              / |             / |
        //             /  |            /  |
        //            /   |           /   |
        //(-1,-1,+1) /____|__________/(+1,-1,+1)
        //           |    |__________|____|
        //           |   /(-1,+1,-1) |    /(+1,+1,-1)
        //           |  /            |   /
        //           | /             |  /
        //           |/              | /
        //           /_______________|/
        //        (-1,-1,-1)       (+1,-1,-1)
        //
        float4 col = float4(1.0, 1.0, 1.0, 1.0);
        Vertex CubeVerts[8] =
        {
            { float3(-0.05f,-0.05f,-0.05f), col },
            { float3(-0.05f,+0.05f,-0.05f), col },
            { float3(+0.05f,+0.05f,-0.05f), col },
            { float3(+0.05f,-0.05f,-0.05f), col },

            { float3(-0.05f,-0.05f,+0.05f), col },
            { float3(-0.05f,+0.05f,+0.05f), col },
            { float3(+0.05f,+0.05f,+0.05f), col },
            { float3(+0.05f,-0.05f,+0.05f), col },
        };

        // Create a vertex buffer that stores cube vertices
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "Cube Light vertex buffer";
        VertBuffDesc.Usage = USAGE_STATIC;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.uiSizeInBytes = sizeof(CubeVerts);
        BufferData VBData;
        VBData.pData = CubeVerts;
        VBData.DataSize = sizeof(CubeVerts);
        m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_pLightCubeVertexBuffer);
        
        Uint32 Indices[] =
        {
            2,0,1, 2,3,0,
            4,6,5, 4,7,6,
            0,7,4, 0,3,7,
            1,0,4, 1,4,5,
            1,5,2, 5,6,2,
            3,6,7, 3,2,6
        };

        BufferDesc IndBuffDesc;
        IndBuffDesc.Name = "Cube index buffer";
        IndBuffDesc.Usage = USAGE_STATIC;
        IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
        IndBuffDesc.uiSizeInBytes = sizeof(Indices);
        BufferData IBData;
        IBData.pData = Indices;
        IBData.DataSize = sizeof(Indices);
        m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_pLightCubeIndexBuffer);
    }

    void EarthMesh::LoadEarthTexture()
    {
        // Load texture
        TextureLoadInfo loadHeightInfo;
        loadHeightInfo.IsSRGB = false;
        loadHeightInfo.Name = "Terrain height map";
        RefCntAutoPtr<ITexture> heightMap;
        CreateTextureFromFile("Terrain\\height.png", loadHeightInfo, m_pDevice, &heightMap);
        // Get shader resource view from the texture
        m_heightMapSRV = heightMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        m_pEarthSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_heightMap")->Set(m_heightMapSRV);

        TextureLoadInfo loadTextInfo;
        loadTextInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> text;
        CreateTextureFromFile("Terrain\\texture.png", loadTextInfo, m_pDevice, &text);
        // Get shader resource view from the texture
        m_textureSRV = text->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        // Set texture SRV in the SRB
        m_pEarthSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_texture")->Set(m_textureSRV);
    }

}