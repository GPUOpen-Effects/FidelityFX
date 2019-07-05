//CAS Sample
//
// Copyright(c) 2019 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once

static const int cNumSwapBufs = 2;

#define USE_VID_MEM true

using namespace CAULDRON_VK;
using namespace CAS_SAMPLE_VK;

//
// This class deals with the GPU side of the sample.
//

class CAS_Renderer
{
public:
    struct Spotlight
    {
        Camera light;
        XMFLOAT4 color;
        float intensity;
    };

    struct State
    {
        float               time;
        Camera              camera;

        float               exposure;
        float               iblFactor;
        float               emmisiveFactor;

        int                 toneMapper;
        int                 skyDomeType;
        bool                bDrawBoundingBoxes;

        uint32_t            spotlightCount;
        Spotlight           spotlight[4];
        bool                bDrawLightFrustum;

        int                 renderWidth;
        int                 renderHeight;
        bool                usePackedMath;
        CAS_State           CASState;
        bool                profiling;
        float               sharpenControl;
    };

    void OnCreate(Device *pDevice, SwapChain *pSwapChain);
    void OnDestroy();

    void OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, State* pState, uint32_t Width, uint32_t Height);
    void OnDestroyWindowSizeDependentResources();

    int LoadScene(GLTFCommon *pGLTFCommon, int stage = 0);
    void UnloadScene();

    const std::vector<TimeStamp> &GetTimingValues() { return m_TimeStamps; }

    void OnRender(State *pState, SwapChain *pSwapChain);

    void UpdateCASSharpness(float sharpenControl, CAS_State CASState);

private:
    Device                         *m_pDevice;

    uint32_t                        m_Width;
    uint32_t                        m_Height;

    VkRect2D                        m_scissor;
    VkRect2D                        m_finalScissor;
    VkViewport                      m_viewport;
    VkViewport                      m_finalViewport;

    // Initialize helper classes
    ResourceViewHeaps               m_resourceViewHeaps;
    UploadHeap                      m_UploadHeap;
    DynamicBufferRing               m_ConstantBufferRing;
    StaticBufferPool                m_VidMemBufferPool;
    StaticBufferPool                m_SysMemBufferPool;
    CommandListRing                 m_CommandListRing;
    GPUTimestamps                   m_GPUTimer;

    //gltf passes
    GLTFTexturesAndBuffers         *m_pGLTFTexturesAndBuffers;
    GltfPbrPass                    *m_pGltfPBR;
    GltfDepthPass                  *m_pGltfDepth;
    GltfBBoxPass                   *m_pGltfBBox;

    // effects
    Bloom                           m_bloom;
    SkyDome                         m_skyDome;
    DownSamplePS                    m_downSample;
    SkyDomeProc                     m_skyDomeProc;
    ToneMapping                     m_toneMapping;
    CAS_Filter                      m_CAS;

    // GUI
    ImGUI                           m_ImGUI;

    // Temporary render targets

    // depth buffer
    Texture                         m_depthBuffer;
    VkImageView                     m_depthBufferView;

    // shadowmaps
    Texture                         m_shadowMap;
    VkImageView                     m_shadowMapDSV;
    VkImageView                     m_shadowMapSRV;

    // MSAA RT
    Texture                         m_HDRMSAA;
    VkImageView                     m_HDRSRV;
    VkImageView                     m_HDRMSAASRV;

    // Resolved RT
    Texture                         m_HDR;
    
    // Tone map RT
    Texture                         m_tonemapTexture;
    VkImageView                     m_tonemapSRV;

    // widgets
    WireframeBox                    m_wireframeBox;

    VkRenderPass                    m_render_pass_shadow;
    VkRenderPass                    m_render_pass_HDR_MSAA;
    VkRenderPass                    m_render_pass_tonemap;
    VkRenderPass                    m_render_pass_swap_chain;

    VkFramebuffer                   m_shadowMapBuffers;
    VkFramebuffer                   m_frameBuffer_HDR_MSAA;
    VkFramebuffer                   m_frameBuffer_tonemap;

    std::vector<TimeStamp>          m_TimeStamps;
};

