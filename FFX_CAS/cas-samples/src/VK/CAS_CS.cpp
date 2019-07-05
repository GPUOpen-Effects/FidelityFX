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

#include "stdafx.h"
#include "CAS_CS.h"

// CAS
#define A_CPU
#include "ffx_a.h"
#include "ffx_cas.h"

namespace CAS_SAMPLE_VK
{
    void CAS_Filter::OnCreate(Device* pDevice, VkRenderPass renderPass, VkFormat outFormat, ResourceViewHeaps *pResourceViewHeaps, StaticBufferPool  *pStaticBufferPool, DynamicBufferRing *pDynamicBufferRing)
    {
        m_pDevice = pDevice;
        m_pDynamicBufferRing = pDynamicBufferRing;
        m_pResourceViewHeaps = pResourceViewHeaps;
        
        {
            VkSamplerCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            info.magFilter = VK_FILTER_LINEAR;
            info.minFilter = VK_FILTER_LINEAR;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            info.minLod = -1000;
            info.maxLod = 1000;
            info.maxAnisotropy = 1.0f;
            VkResult res = vkCreateSampler(m_pDevice->GetDevice(), &info, NULL, &m_renderSampler);
            assert(res == VK_SUCCESS);
        }

        {
            std::vector<VkDescriptorSetLayoutBinding> layoutBindings(3);
            layoutBindings[0].binding = 0;
            layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            layoutBindings[0].descriptorCount = 1;
            layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            layoutBindings[0].pImmutableSamplers = NULL;

            layoutBindings[1].binding = 1;
            layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            layoutBindings[1].descriptorCount = 1;
            layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            layoutBindings[1].pImmutableSamplers = NULL;

            layoutBindings[2].binding = 2;
            layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            layoutBindings[2].descriptorCount = 1;
            layoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            layoutBindings[2].pImmutableSamplers = NULL;

            m_pResourceViewHeaps->CreateDescriptorSetLayoutAndAllocDescriptorSet(&layoutBindings, &m_upscaleDescriptorSetLayout, &m_upscaleDescriptorSet);
            m_pDynamicBufferRing->SetDescriptorSet(0, sizeof(uint32_t) * 12, m_upscaleDescriptorSet);

            DefineList defines;
            defines["CAS_SAMPLE_FP16"] = "0";
            defines["CAS_SAMPLE_SHARPEN_ONLY"] = "0";

            if (pDevice->IsFp16Supported())
            {
                defines["CAS_SAMPLE_FP16"] = "1";

                defines["CAS_SAMPLE_SHARPEN_ONLY"] = "1";
                m_casPackedSharpenOnly.OnCreate(pDevice, "CAS_Shader.glsl", "main", m_upscaleDescriptorSetLayout, 64, 1, 1, &defines);

                defines["CAS_SAMPLE_SHARPEN_ONLY"] = "0";
                m_casPackedUpsample.OnCreate(pDevice, "CAS_Shader.glsl", "main", m_upscaleDescriptorSetLayout, 64, 1, 1, &defines);
            }

            defines["CAS_SAMPLE_FP16"] = "0";

            defines["CAS_SAMPLE_SHARPEN_ONLY"] = "1";
            m_casSharpenOnly.OnCreate(pDevice, "CAS_Shader.glsl", "main", m_upscaleDescriptorSetLayout, 64, 1, 1, &defines);

            defines["CAS_SAMPLE_SHARPEN_ONLY"] = "0";
            m_casUpsample.OnCreate(pDevice, "CAS_Shader.glsl", "main", m_upscaleDescriptorSetLayout, 64, 1, 1, &defines);
        }

        {
            std::vector<VkDescriptorSetLayoutBinding> layoutBindings(1);
            layoutBindings[0].binding = 0;
            layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layoutBindings[0].descriptorCount = 1;
            layoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layoutBindings[0].pImmutableSamplers = NULL;

            m_pResourceViewHeaps->CreateDescriptorSetLayoutAndAllocDescriptorSet(&layoutBindings, &m_renderDescriptorSetLayout, &m_renderSrcSRVDescriptorSet);
            m_pResourceViewHeaps->CreateDescriptorSetLayoutAndAllocDescriptorSet(&layoutBindings, &m_renderDescriptorSetLayout, &m_renderDstSRVDescriptorSet);
            m_renderFullscreen.OnCreate(pDevice, renderPass, "CAS_RenderPS.glsl", pStaticBufferPool, pDynamicBufferRing, m_renderDescriptorSetLayout);
        }

        m_dstLayoutUndefined = false;
    }

    void CAS_Filter::OnDestroy()
    {
        m_casUpsample.OnDestroy();
        m_casSharpenOnly.OnDestroy();
        if (m_pDevice->IsFp16Supported())
        {
            m_casPackedUpsample.OnDestroy();
            m_casPackedSharpenOnly.OnDestroy();
        }
        m_renderFullscreen.OnDestroy();

        m_pResourceViewHeaps->FreeDescriptor(m_upscaleDescriptorSet);
        m_pResourceViewHeaps->FreeDescriptor(m_renderSrcSRVDescriptorSet);
        m_pResourceViewHeaps->FreeDescriptor(m_renderDstSRVDescriptorSet);

        vkDestroySampler(m_pDevice->GetDevice(), m_renderSampler, nullptr);
        vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_upscaleDescriptorSetLayout, NULL);
        vkDestroyDescriptorSetLayout(m_pDevice->GetDevice(), m_renderDescriptorSetLayout, NULL);
    }

    void CAS_Filter::OnCreateWindowSizeDependentResources(uint32_t renderWidth, uint32_t renderHeight, uint32_t Width, uint32_t Height, VkImageView srcImgView, CAS_State CASState, bool packedMathEnabled)
    {
        m_renderWidth = renderWidth;
        m_renderHeight = renderHeight;
        m_width = Width;
        m_height = Height;

        // Create upsample target
        {
            VkImageCreateInfo textureDesc = {};
            textureDesc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            textureDesc.pNext = 0;
            textureDesc.flags = 0;
            textureDesc.imageType = VK_IMAGE_TYPE_2D;
            textureDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            textureDesc.extent.width = Width;
            textureDesc.extent.height = Height;
            textureDesc.extent.depth = 1;
            textureDesc.mipLevels = 1;
            textureDesc.arrayLayers = 1;
            textureDesc.samples = VK_SAMPLE_COUNT_1_BIT;
            textureDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
            textureDesc.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            textureDesc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            textureDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            m_dstTexture.Init(m_pDevice, &textureDesc);
            m_dstTexture.CreateSRV(&m_dstTextureSRV);
            m_dstLayoutUndefined = true;

            UpdateSharpness(m_sharpenVal, CASState);
        }

        // Write CAS descriptor set
        {
            VkDescriptorImageInfo ImgInfos[2] = {};
            VkWriteDescriptorSet SetWrites[2] = {};

            // Source img
            ImgInfos[0].sampler = VK_NULL_HANDLE;
            ImgInfos[0].imageView = srcImgView;
            ImgInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            SetWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            SetWrites[0].dstSet = m_upscaleDescriptorSet;
            SetWrites[0].dstBinding = 1;
            SetWrites[0].descriptorCount = 1;
            SetWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            SetWrites[0].pImageInfo = ImgInfos + 0;

            // Dst img
            ImgInfos[1].sampler = VK_NULL_HANDLE;
            ImgInfos[1].imageView = m_dstTextureSRV;
            ImgInfos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            SetWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            SetWrites[1].dstSet = m_upscaleDescriptorSet;
            SetWrites[1].dstBinding = 2;
            SetWrites[1].descriptorCount = 1;
            SetWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            SetWrites[1].pImageInfo = ImgInfos + 1;

            vkUpdateDescriptorSets(m_pDevice->GetDevice(), _countof(SetWrites), SetWrites, 0, 0);
        }

        // Write render src img descriptor set
        {
            VkDescriptorImageInfo ImgInfo = {};
            ImgInfo.sampler = m_renderSampler;
            ImgInfo.imageView = srcImgView;
            ImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet SetWrite = {};
            SetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            SetWrite.dstSet = m_renderSrcSRVDescriptorSet;
            SetWrite.dstBinding = 0;
            SetWrite.descriptorCount = 1;
            SetWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            SetWrite.pImageInfo = &ImgInfo;

            vkUpdateDescriptorSets(m_pDevice->GetDevice(), 1, &SetWrite, 0, 0);
        }

        // Write render CAS img descriptor set
        {
            VkDescriptorImageInfo ImgInfo = {};
            ImgInfo.sampler = m_renderSampler;
            ImgInfo.imageView = m_dstTextureSRV;
            ImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet SetWrite = {};
            SetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            SetWrite.dstSet = m_renderDstSRVDescriptorSet;
            SetWrite.dstBinding = 0;
            SetWrite.descriptorCount = 1;
            SetWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            SetWrite.pImageInfo = &ImgInfo;

            vkUpdateDescriptorSets(m_pDevice->GetDevice(), 1, &SetWrite, 0, 0);
        }
    }

    void CAS_Filter::OnDestroyWindowSizeDependentResources()
    {
        m_dstTexture.OnDestroy();
        vkDestroyImageView(m_pDevice->GetDevice(), m_dstTextureSRV, nullptr);
    }

    void CAS_Filter::Upscale(VkCommandBuffer cmd_buf, Texture srcImg, VkImageView srcImgView, bool useCas, bool usePacked, CAS_State casState)
    {
        if (m_dstLayoutUndefined)
        {
            m_dstLayoutUndefined = false;

            // The dst texture was just created, so we transition it out of its undefined layout
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.image = m_dstTexture.Resource();
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }

        VkDescriptorBufferInfo constsHandle;
        uint32_t* pConstMem;
        m_pDynamicBufferRing->AllocConstantBuffer(sizeof(CASConstants), reinterpret_cast<void **>(&pConstMem), &constsHandle);
        memcpy(pConstMem, &m_consts, sizeof(CASConstants));

        if (useCas)
        {
            // Transition output texture from shader read to general layout
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.image = m_dstTexture.Resource();
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                
                vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
            }

            // This value is the image region dim that each thread group of the CAS shader operates on
            static const int threadGroupWorkRegionDim = 16;
            int dispatchX = (m_width + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
            int dispatchY = (m_height + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;

            if (usePacked)
            {
                if (casState == CAS_State_SharpenOnly)
                {
                    m_casPackedSharpenOnly.Draw(cmd_buf, constsHandle, m_upscaleDescriptorSet, dispatchX, dispatchY, 1);
                }
                else if (casState == CAS_State_Upsample)
                {
                    m_casPackedUpsample.Draw(cmd_buf, constsHandle, m_upscaleDescriptorSet, dispatchX, dispatchY, 1);
                }
            }
            else
            {
                if (casState == CAS_State_SharpenOnly)
                {
                    m_casSharpenOnly.Draw(cmd_buf, constsHandle, m_upscaleDescriptorSet, dispatchX, dispatchY, 1);
                }
                else if (casState == CAS_State_Upsample)
                {
                    m_casUpsample.Draw(cmd_buf, constsHandle, m_upscaleDescriptorSet, dispatchX, dispatchY, 1);
                }
            }

            // Transition dstImg from UAV to ps texture
            {
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.image = m_dstTexture.Resource();
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;

                vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
            }
        }

        // Transition srcImg from UAV to ps texture
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.image = srcImg.Resource();
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }
    }

    void CAS_Filter::DrawToSwapChain(VkCommandBuffer cmd_buf, VkImageView srcImgView, bool useCas)
    {
        if (useCas)
        {
            // Render to swap chain
            m_renderFullscreen.Draw(cmd_buf, {}, m_renderDstSRVDescriptorSet);
        }
        else
        {
            m_renderFullscreen.Draw(cmd_buf, {}, m_renderSrcSRVDescriptorSet);
        }
    }

    void CAS_Filter::UpdateSharpness(float NewSharpenVal, CAS_State CASState)
    {
        AF1 outWidth = static_cast<AF1>((CASState == CAS_State_Upsample) ? m_width : m_renderWidth);
        AF1 outHeight = static_cast<AF1>((CASState == CAS_State_Upsample) ? m_height : m_renderHeight);

        CasSetup(reinterpret_cast<AU1*>(&m_consts.Const0), reinterpret_cast<AU1*>(&m_consts.Const1), m_sharpenVal, static_cast<AF1>(m_renderWidth),
            static_cast<AF1>(m_renderHeight), outWidth, outHeight);
        m_sharpenVal = NewSharpenVal;
    }

    static const ResolutionInfo s_CommonResolutions[] =
    {
        { "480p", 640, 480 },
        { "720p", 1280, 720 },
        { "1080p", 1920, 1080 },
        { "1440p", 2560, 1440 },
    };

    void CAS_Filter::GetSupportedResolutions(uint32_t displayWidth, uint32_t displayHeight, std::vector<ResolutionInfo>& supportedList)
    {
        // Check which of the fixed resolutions we support rendering to with CAS enabled
        for (uint32_t iRes = 0; iRes < _countof(s_CommonResolutions); ++iRes)
        {
            ResolutionInfo currResolution = s_CommonResolutions[iRes];
            if (CasSupportScaling(static_cast<AF1>(displayWidth), static_cast<AF1>(displayHeight), static_cast<AF1>(currResolution.Width), static_cast<AF1>(currResolution.Height)) &&
                currResolution.Width < displayWidth && currResolution.Height < displayHeight)
            {
                supportedList.push_back(currResolution);
            }
        }

        // Also add the display res as a supported render resolution
        ResolutionInfo displayResInfo = {};
        displayResInfo.pName = "Display Res";
        displayResInfo.Width = displayWidth;
        displayResInfo.Height = displayHeight;
        supportedList.push_back(displayResInfo);
    }
}