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

#include "PostProc/PostProcCS.h"
#include "PostProc/PostProcPS.h"

using namespace CAULDRON_VK;

namespace CAS_SAMPLE_VK
{
    enum CAS_State
    {
        CAS_State_NoCas,
        CAS_State_Upsample,
        CAS_State_SharpenOnly,
    };

    struct ResolutionInfo
    {
        const char* pName;
        uint32_t Width;
        uint32_t Height;
    };

    struct CASConstants
    {
        XMUINT4 Const0;
        XMUINT4 Const1;
    };

    class CAS_Filter
    {
    public:
        void OnCreate(Device* pDevice, VkRenderPass renderPass, VkFormat outFormat, ResourceViewHeaps *pResourceViewHeaps, StaticBufferPool  *pStaticBufferPool, DynamicBufferRing *pDynamicBufferRing);
        void OnDestroy();

        void OnCreateWindowSizeDependentResources(uint32_t renderWidth, uint32_t renderHeight, uint32_t Width, uint32_t Height, VkImageView srcImgView, CAS_State CASState, bool packedMathEnabled);
        void OnDestroyWindowSizeDependentResources();

        void Upscale(VkCommandBuffer cmd_buf, Texture srcImg, VkImageView srcImgView, bool useCas, bool usePacked, CAS_State casState);
        void DrawToSwapChain(VkCommandBuffer cmd_buf, VkImageView srcImgView, bool useCas);

        void UpdateSharpness(float NewSharpen, CAS_State CASState);

        static void GetSupportedResolutions(uint32_t displayWidth, uint32_t displayHeight, std::vector<ResolutionInfo>& supportedList);

    private:
        Device                         *m_pDevice;
        ResourceViewHeaps              *m_pResourceViewHeaps;

        float                           m_sharpenVal;
        uint32_t                        m_renderWidth;
        uint32_t                        m_renderHeight;
        uint32_t                        m_width;
        uint32_t                        m_height;
        CASConstants                    m_consts;

        PostProcCS                      m_casSharpenOnly;
        PostProcCS                      m_casUpsample;
        PostProcCS                      m_casPackedSharpenOnly;
        PostProcCS                      m_casPackedUpsample;
        PostProcPS                      m_renderFullscreen;

        DynamicBufferRing              *m_pDynamicBufferRing = NULL;

        Texture                         m_dstTexture;
        VkImageView                     m_dstTextureSRV;
        VkSampler                       m_renderSampler; // Samples dst texture for rendering to swap chain

        VkDescriptorSet                 m_upscaleDescriptorSet;
        VkDescriptorSetLayout           m_upscaleDescriptorSetLayout;

        VkDescriptorSet                 m_renderDstSRVDescriptorSet;
        VkDescriptorSet                 m_renderSrcSRVDescriptorSet;
        VkDescriptorSetLayout           m_renderDescriptorSetLayout;

        bool                            m_dstLayoutUndefined;
    };
}
