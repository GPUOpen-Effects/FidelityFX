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

#include "CAS_Renderer.h"

//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void CAS_Renderer::OnCreate(Device *pDevice, SwapChain *pSwapChain)
{
    m_pDevice = pDevice;

    // Initialize helpers

    // Create all the heaps for the resources views
    m_resourceViewHeaps.OnCreate(pDevice, 2000, 2000, 10, 2000);

    // Create a commandlist ring for the Direct queue
    m_CommandListRing.OnCreate(pDevice, cNumSwapBufs, 8);

    // Create a 'dynamic' constant buffers ring
    m_ConstantBufferRing.OnCreate(pDevice, cNumSwapBufs, 20 * 1024 * 1024, "Uniforms");

    // Create a 'static' constant buffer pool
    m_VidMemBufferPool.OnCreate(pDevice, 128 * 1024 * 1024, USE_VID_MEM, "StaticGeom");
    m_SysMemBufferPool.OnCreate(pDevice, 32 * 1024 , false, "PostProcGeom");

    // initialize the GPU time stamps module
    m_GPUTimer.OnCreate(pDevice, cNumSwapBufs);

    // Quick helper to upload resources, it has it's own commandList and uses suballocation.
    // for 4K textures we'll need 100Megs
    m_UploadHeap.OnCreate(pDevice, 100 * 1024 * 1024);    // initialize an upload heap (uses suballocation for faster results)

    // Create a Shadowmap atlas to hold 4 cascades/spotlights
    m_shadowMap.InitDepthStencil(m_pDevice, 2*1024, 2 * 1024, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, "ShadowMap");
    m_shadowMap.CreateSRV(&m_shadowMapSRV);
    m_shadowMap.CreateDSV(&m_shadowMapDSV);
    // Shadow RT
    {
        VkImageMemoryBarrier currBarrier = {};
        currBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        currBarrier.srcAccessMask = 0;
        currBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        currBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        currBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        currBarrier.image = m_shadowMap.Resource();
        currBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        currBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        currBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        currBarrier.subresourceRange.baseMipLevel = 0;
        currBarrier.subresourceRange.levelCount = 1;
        currBarrier.subresourceRange.baseArrayLayer = 0;
        currBarrier.subresourceRange.layerCount = 1;
    }

    // Create render pass shadow
    //
    {
        /* Need attachments for render target and depth buffer */
        VkAttachmentDescription attachments[1];

        // depth RT
        attachments[0].format = m_shadowMap.GetFormat();
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments[0].flags = 0;

        VkAttachmentReference depth_reference = { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.flags = 0;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = NULL;
        subpass.colorAttachmentCount = 0;
        subpass.pColorAttachments = NULL;
        subpass.pResolveAttachments = NULL;
        subpass.pDepthStencilAttachment = &depth_reference;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = NULL;

        VkRenderPassCreateInfo rp_info = {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.pNext = NULL;
        rp_info.attachmentCount = 1;
        rp_info.pAttachments = attachments;
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;
        rp_info.dependencyCount = 0;
        rp_info.pDependencies = NULL;

        VkResult res = vkCreateRenderPass(m_pDevice->GetDevice(), &rp_info, NULL, &m_render_pass_shadow);
        assert(res == VK_SUCCESS);
        
        // Create frame buffer
        //
        VkImageView attachmentViews[1] = { m_shadowMapDSV };
        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.pNext = NULL;
        fb_info.renderPass = m_render_pass_shadow;
        fb_info.attachmentCount = 1;
        fb_info.pAttachments = attachmentViews;
        fb_info.width = m_shadowMap.GetWidth();
        fb_info.height = m_shadowMap.GetHeight();
        fb_info.layers = 1;
        res = vkCreateFramebuffer(m_pDevice->GetDevice(), &fb_info, NULL, &m_shadowMapBuffers);
        assert(res == VK_SUCCESS);
    }

    // Create HDR MSAA render pass color with clear
    //
    {
        /* Need attachments for render target and depth buffer */
        VkAttachmentDescription attachments[2];

        // color MSAA RT
        attachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
        attachments[0].samples = VK_SAMPLE_COUNT_4_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        attachments[0].flags = 0;

        // depth RT
        attachments[1].format = VK_FORMAT_D32_SFLOAT;
        attachments[1].samples = VK_SAMPLE_COUNT_4_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[1].flags = 0;

        VkAttachmentReference color_reference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference depth_reference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.flags = 0;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = NULL;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_reference;
        subpass.pResolveAttachments = NULL;
        subpass.pDepthStencilAttachment = &depth_reference;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = NULL;

        VkRenderPassCreateInfo rp_info = {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.pNext = NULL;
        rp_info.attachmentCount = 2;
        rp_info.pAttachments = attachments;
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;
        rp_info.dependencyCount = 0;
        rp_info.pDependencies = NULL;

        VkResult res = vkCreateRenderPass(m_pDevice->GetDevice(), &rp_info, NULL, &m_render_pass_HDR_MSAA);
        assert(res == VK_SUCCESS);
    }

    // Create tone map render pass 
    {
        // color RT
        VkAttachmentDescription attachments[1];
        attachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
        attachments[0].flags = 0;

        VkAttachmentReference color_reference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.flags = 0;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = NULL;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_reference;
        subpass.pResolveAttachments = NULL;
        subpass.pDepthStencilAttachment = NULL;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = NULL;

        // Transition tone mapping output to shader read layout for CAS/outputting to swap chain 
        VkSubpassDependency subpassDependency = {};
        subpassDependency.srcSubpass = 0;
        subpassDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        subpassDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        subpassDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        subpassDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo rp_info = {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.pNext = NULL;
        rp_info.attachmentCount = 1;
        rp_info.pAttachments = attachments;
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;
        rp_info.dependencyCount = 1;
        rp_info.pDependencies = &subpassDependency;

        VkResult res = vkCreateRenderPass(m_pDevice->GetDevice(), &rp_info, NULL, &m_render_pass_tonemap);
        assert(res == VK_SUCCESS);
    }

    // Create swapchain render pass 
    {
        // color RT
        VkAttachmentDescription attachments[1];
        attachments[0].format = pSwapChain->GetFormat();
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments[0].flags = 0;

        VkAttachmentReference color_reference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.flags = 0;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = NULL;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_reference;
        subpass.pResolveAttachments = NULL;
        subpass.pDepthStencilAttachment = NULL;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = NULL;

        VkRenderPassCreateInfo rp_info = {};
        rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.pNext = NULL;
        rp_info.attachmentCount = 1;
        rp_info.pAttachments = attachments;
        rp_info.subpassCount = 1;
        rp_info.pSubpasses = &subpass;
        rp_info.dependencyCount = 0;
        rp_info.pDependencies = NULL;

        VkResult res = vkCreateRenderPass(m_pDevice->GetDevice(), &rp_info, NULL, &m_render_pass_swap_chain);
        assert(res == VK_SUCCESS);
    }

    m_skyDome.OnCreate(pDevice, m_render_pass_HDR_MSAA, &m_UploadHeap, VK_FORMAT_R16G16B16A16_SFLOAT, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, "..\\..\\cauldron-media\\envmaps\\papermill\\diffuse.dds", "..\\..\\cauldron-media\\envmaps\\papermill\\specular.dds", VK_SAMPLE_COUNT_4_BIT);
    m_skyDomeProc.OnCreate(pDevice, m_render_pass_HDR_MSAA, &m_UploadHeap, VK_FORMAT_R16G16B16A16_SFLOAT, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_SAMPLE_COUNT_4_BIT);
    m_wireframeBox.OnCreate(pDevice, m_render_pass_HDR_MSAA, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_SAMPLE_COUNT_4_BIT);
    m_downSample.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_FORMAT_R16G16B16A16_SFLOAT);
    m_bloom.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, VK_FORMAT_R16G16B16A16_SFLOAT);

    // Create tonemapping pass
    m_toneMapping.OnCreate(m_pDevice, m_render_pass_tonemap, &m_resourceViewHeaps, &m_SysMemBufferPool, &m_ConstantBufferRing);

    // Create cas pass
    m_CAS.OnCreate(m_pDevice, m_render_pass_swap_chain, pSwapChain->GetFormat(), &m_resourceViewHeaps, &m_SysMemBufferPool, &m_ConstantBufferRing);

    // Initialize UI rendering resources
    m_ImGUI.OnCreate(m_pDevice, m_render_pass_swap_chain, &m_UploadHeap, &m_ConstantBufferRing);

    // Make sure upload heap has finished uploading before continuing
#if (USE_VID_MEM==true)
    m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
    m_UploadHeap.FlushAndFinish();
#endif
}

//--------------------------------------------------------------------------------------
//
// OnDestroy 
//
//--------------------------------------------------------------------------------------
void CAS_Renderer::OnDestroy()
{   
    m_toneMapping.OnDestroy();
    m_ImGUI.OnDestroy();
    m_bloom.OnDestroy();
    m_downSample.OnDestroy();
    m_wireframeBox.OnDestroy();
    m_skyDomeProc.OnDestroy();
    m_skyDome.OnDestroy();
    m_shadowMap.OnDestroy();
    m_CAS.OnDestroy();

    vkDestroyImageView(m_pDevice->GetDevice(), m_shadowMapDSV, nullptr);
    vkDestroyImageView(m_pDevice->GetDevice(), m_shadowMapSRV, nullptr);
    
    vkDestroyRenderPass(m_pDevice->GetDevice(), m_render_pass_shadow, nullptr);
    vkDestroyRenderPass(m_pDevice->GetDevice(), m_render_pass_HDR_MSAA, nullptr);
    vkDestroyRenderPass(m_pDevice->GetDevice(), m_render_pass_tonemap, nullptr);
    vkDestroyRenderPass(m_pDevice->GetDevice(), m_render_pass_swap_chain, nullptr);

    vkDestroyFramebuffer(m_pDevice->GetDevice(), m_shadowMapBuffers, nullptr);
       
    m_UploadHeap.OnDestroy();
    m_GPUTimer.OnDestroy();
    m_VidMemBufferPool.OnDestroy();
    m_SysMemBufferPool.OnDestroy();
    m_ConstantBufferRing.OnDestroy();
    m_resourceViewHeaps.OnDestroy();
    m_CommandListRing.OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// OnCreateWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void CAS_Renderer::OnCreateWindowSizeDependentResources(SwapChain *pSwapChain, State* pState, uint32_t Width, uint32_t Height)
{
    // Set resolution data
    m_Width = Width;
    m_Height = Height;

    int targetWidth = m_Width;
    int targetHeight = m_Height;

    if (pState->CASState == CAS_State_SharpenOnly)
    {
        targetWidth = pState->renderWidth;
        targetHeight = pState->renderHeight;
    }
    // Set the viewport
    //
    m_viewport.x = 0;
    m_viewport.y = static_cast<float>(pState->renderHeight);
    m_viewport.width = static_cast<float>(pState->renderWidth);
    m_viewport.height = -static_cast<float>(pState->renderHeight);
    m_viewport.minDepth = static_cast<float>(0.0f);
    m_viewport.maxDepth = static_cast<float>(1.0f);

    m_finalViewport.x = 0;
    m_finalViewport.y = static_cast<float>(Height);
    m_finalViewport.width = static_cast<float>(Width);
    m_finalViewport.height = -static_cast<float>(Height);
    m_finalViewport.minDepth = static_cast<float>(0.0f);
    m_finalViewport.maxDepth = static_cast<float>(1.0f);

    // Create scissor rectangle
    //
    m_scissor.extent.width = pState->renderWidth;
    m_scissor.extent.height = pState->renderHeight;
    m_scissor.offset.x = 0;
    m_scissor.offset.y = 0;

    // Create scissor rectangle
    //
    m_finalScissor.extent.width = Width;
    m_finalScissor.extent.height = Height;
    m_finalScissor.offset.x = 0;
    m_finalScissor.offset.y = 0;

    // Create depth buffer
    //
    {
        m_depthBuffer.InitDepthStencil(m_pDevice, Width, Height, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_4_BIT, "DepthBuffer");
        m_depthBuffer.CreateDSV(&m_depthBufferView);

        VkImageMemoryBarrier currBarrier = {};
        currBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        currBarrier.srcAccessMask = 0;
        currBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        currBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        currBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        currBarrier.image = m_depthBuffer.Resource();
        currBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        currBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        currBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        currBarrier.subresourceRange.baseMipLevel = 0;
        currBarrier.subresourceRange.levelCount = 1;
        currBarrier.subresourceRange.baseArrayLayer = 0;
        currBarrier.subresourceRange.layerCount = 1;
    }

    // Create Texture + RTV with x4 MSAA
    //
    {
        m_HDRMSAA.InitRendertarget(m_pDevice, pState->renderWidth, pState->renderHeight, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_4_BIT, static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT), false, "HDRMSAA");
        m_HDRMSAA.CreateRTV(&m_HDRMSAASRV);

        VkImageMemoryBarrier currBarrier = {};
        currBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        currBarrier.srcAccessMask = 0;
        currBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        currBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        currBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        currBarrier.image = m_HDRMSAA.Resource();
        currBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        currBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        currBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        currBarrier.subresourceRange.baseMipLevel = 0;
        currBarrier.subresourceRange.levelCount = 1;
        currBarrier.subresourceRange.baseArrayLayer = 0;
        currBarrier.subresourceRange.layerCount = 1;
    }

    // Create Texture + RTV, to hold the resolved scene 
    //
    {
        m_HDR.InitRendertarget(m_pDevice, pState->renderWidth, pState->renderHeight, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT), false, "HDR");
        m_HDR.CreateSRV(&m_HDRSRV);
    }

    // Create Texture + RTV, to hold the tonemapped scene
    //
    {
        m_tonemapTexture.InitRendertarget(m_pDevice, pState->renderWidth, pState->renderHeight, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT), false, "Tonemap");
        m_tonemapTexture.CreateSRV(&m_tonemapSRV);

        VkImageMemoryBarrier currBarrier = {};
        currBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        currBarrier.srcAccessMask = 0;
        currBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        currBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        currBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        currBarrier.image = m_tonemapTexture.Resource();
        currBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        currBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        currBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        currBarrier.subresourceRange.baseMipLevel = 0;
        currBarrier.subresourceRange.levelCount = 1;
        currBarrier.subresourceRange.baseArrayLayer = 0;
        currBarrier.subresourceRange.layerCount = 1;
    }

    // Create framebuffer for the MSAA RT
    //
    {
        VkImageView attachments[2] = { m_HDRMSAASRV, m_depthBufferView };

        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.pNext = NULL;
        fb_info.renderPass = m_render_pass_HDR_MSAA;
        fb_info.attachmentCount = 2;
        fb_info.pAttachments = attachments;
        fb_info.width = pState->renderWidth;
        fb_info.height = pState->renderHeight;
        fb_info.layers = 1;

        VkResult res = vkCreateFramebuffer(m_pDevice->GetDevice(), &fb_info, NULL, &m_frameBuffer_HDR_MSAA);
        assert(res == VK_SUCCESS);
    }

    // Create framebuffer for the tonemapped scene
    //
    {
        VkImageView attachments[1] = { m_tonemapSRV };

        VkFramebufferCreateInfo fb_info = {};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.pNext = NULL;
        fb_info.renderPass = m_render_pass_tonemap;
        fb_info.attachmentCount = 1;
        fb_info.pAttachments = attachments;
        fb_info.width = pState->renderWidth;
        fb_info.height = pState->renderHeight;
        fb_info.layers = 1;

        VkResult res = vkCreateFramebuffer(m_pDevice->GetDevice(), &fb_info, NULL, &m_frameBuffer_tonemap);
        assert(res == VK_SUCCESS);
    }

    // Update bloom, downscaling, tonemapping and CAS
    //
    m_downSample.OnCreateWindowSizeDependentResources(pState->renderWidth, pState->renderHeight, &m_HDR, 5); //downsample the HDR texture 5 times
    m_bloom.OnCreateWindowSizeDependentResources(pState->renderWidth / 2, pState->renderHeight / 2, m_downSample.GetTexture(), 5, &m_HDR);
    m_toneMapping.OnCreateWindowSizeDependentResources(m_HDRSRV);
    m_CAS.OnCreateWindowSizeDependentResources(pState->renderWidth, pState->renderHeight, targetWidth, targetHeight, m_tonemapSRV, pState->CASState, pState->usePackedMath);

    m_UploadHeap.FlushAndFinish();
}

//--------------------------------------------------------------------------------------
//
// OnDestroyWindowSizeDependentResources
//
//--------------------------------------------------------------------------------------
void CAS_Renderer::OnDestroyWindowSizeDependentResources()
{
    m_bloom.OnDestroyWindowSizeDependentResources();
    m_downSample.OnDestroyWindowSizeDependentResources();
    m_CAS.OnDestroyWindowSizeDependentResources();

    m_HDR.OnDestroy();
    m_HDRMSAA.OnDestroy();
    m_depthBuffer.OnDestroy();

    vkDestroyFramebuffer(m_pDevice->GetDevice(), m_frameBuffer_HDR_MSAA, nullptr);
    vkDestroyFramebuffer(m_pDevice->GetDevice(), m_frameBuffer_tonemap, nullptr);

    vkDestroyImageView(m_pDevice->GetDevice(), m_depthBufferView, nullptr);
    vkDestroyImageView(m_pDevice->GetDevice(), m_HDRMSAASRV, nullptr);
    vkDestroyImageView(m_pDevice->GetDevice(), m_HDRSRV, nullptr);
    m_tonemapTexture.OnDestroy();
    vkDestroyImageView(m_pDevice->GetDevice(), m_tonemapSRV, nullptr);
}

//--------------------------------------------------------------------------------------
//
// LoadScene
//
//--------------------------------------------------------------------------------------
int CAS_Renderer::LoadScene(GLTFCommon *pGLTFCommon, int stage)
{
    // show loading progress
    //
    ImGui::OpenPopup("Loading");
    if (ImGui::BeginPopupModal("Loading", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        float progress = static_cast<float>(stage) / 12.0f;
        ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), NULL);
        ImGui::EndPopup();
    } 

    // Loading stages
    //
    if (stage == 0)
    {
    }
    else if (stage == 5)
    {   
        Profile p("m_pGltfLoader->Load");
        
        m_pGLTFTexturesAndBuffers = new GLTFTexturesAndBuffers();
        m_pGLTFTexturesAndBuffers->OnCreate(m_pDevice, pGLTFCommon, &m_UploadHeap, &m_VidMemBufferPool, &m_ConstantBufferRing);
    }
    else if (stage == 6)
    {
        Profile p("LoadTextures");

        // here we are loading onto the GPU all the textures and the inverse matrices
        // this data will be used to create the PBR and Depth passes       
        m_pGLTFTexturesAndBuffers->LoadTextures();
    }
    else if (stage == 7)
    {
        Profile p("m_gltfDepth->OnCreate");

        //create the glTF's textures, VBs, IBs, shaders and descriptors for this particular pass    
        m_pGltfDepth = new GltfDepthPass();
        m_pGltfDepth->OnCreate(
            m_pDevice,
            m_render_pass_shadow,
            &m_UploadHeap,
            &m_resourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers
        );
#if (USE_VID_MEM==true)
        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
        m_UploadHeap.FlushAndFinish();
#endif
    }
    else if (stage == 8)
    {
        Profile p("m_gltfPBR->OnCreate");

        // same thing as above but for the PBR pass
        m_pGltfPBR = new GltfPbrPass();
        m_pGltfPBR->OnCreate(
        m_pDevice,
        m_render_pass_HDR_MSAA,
        &m_UploadHeap,
        &m_resourceViewHeaps,
        &m_ConstantBufferRing,
        &m_VidMemBufferPool,
        m_pGLTFTexturesAndBuffers,
        &m_skyDome,
        m_shadowMapSRV,
        VK_SAMPLE_COUNT_4_BIT
        );
#if (USE_VID_MEM==true)
        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
        m_UploadHeap.FlushAndFinish();
#endif
    }
    else if (stage == 9)
    {
        Profile p("m_gltfBBox->OnCreate");

        // just a bounding box pass that will draw boundingboxes instead of the geometry itself
        m_pGltfBBox = new GltfBBoxPass();
        m_pGltfBBox->OnCreate(
        m_pDevice,
        m_render_pass_HDR_MSAA,
        &m_resourceViewHeaps,
        &m_ConstantBufferRing,
        &m_VidMemBufferPool,
        m_pGLTFTexturesAndBuffers,
        VK_SAMPLE_COUNT_4_BIT
        );
#if (USE_VID_MEM==true)
        // we are borrowing the upload heap command list for uploading to the GPU the IBs and VBs
        m_VidMemBufferPool.UploadData(m_UploadHeap.GetCommandList());
#endif
    }
    else if (stage == 10)
    {
        Profile p("Flush");

        m_UploadHeap.FlushAndFinish();

#if (USE_VID_MEM==true)
        //once everything is uploaded we dont need he upload heaps anymore
        m_VidMemBufferPool.FreeUploadHeap();
#endif    
        // tell caller that we are done loading the map
        return -1;
    }

    stage++;
    return stage;
}

//--------------------------------------------------------------------------------------
//
// UnloadScene
//
//--------------------------------------------------------------------------------------
void CAS_Renderer::UnloadScene()
{
    if (m_pGltfPBR)
    {
        m_pGltfPBR->OnDestroy();
        delete m_pGltfPBR;
        m_pGltfPBR = NULL;
    }

    if (m_pGltfDepth)
    {
        m_pGltfDepth->OnDestroy();
        delete m_pGltfDepth;
        m_pGltfDepth = NULL;
    }

    if (m_pGltfBBox)
    {
        m_pGltfBBox->OnDestroy();
        delete m_pGltfBBox;
        m_pGltfBBox = NULL;
    }

    if (m_pGLTFTexturesAndBuffers)
    {
        m_pGLTFTexturesAndBuffers->OnDestroy();
        delete m_pGLTFTexturesAndBuffers;
        m_pGLTFTexturesAndBuffers = NULL;
    }
}

//--------------------------------------------------------------------------------------
//
// OnRender
//
//--------------------------------------------------------------------------------------
void CAS_Renderer::OnRender(State *pState, SwapChain *pSwapChain)
{
    // Let our resource managers do some house keeping 
    //
    m_ConstantBufferRing.OnBeginFrame();
    m_CommandListRing.OnBeginFrame();

    // command buffer calls
    //
    VkCommandBuffer cmd_buf = m_CommandListRing.GetNewCommandList();

    {
        VkCommandBufferBeginInfo cmd_buf_info;
        cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_info.pNext = NULL;
        cmd_buf_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        cmd_buf_info.pInheritanceInfo = NULL;
        VkResult res = vkBeginCommandBuffer(cmd_buf, &cmd_buf_info);
        assert(res == VK_SUCCESS);
    }

    m_GPUTimer.OnBeginFrame(cmd_buf, &m_TimeStamps);

    // Sets the perFrame data (Camera and lights data), override as necessary and set them as constant buffers --------------
    //
    per_frame *pPerFrame = NULL;
    if (m_pGLTFTexturesAndBuffers)
    {
        pPerFrame = m_pGLTFTexturesAndBuffers->m_pGLTFCommon->SetPerFrameData(0);

        //override gltf camera with ours
        pPerFrame->mCameraViewProj = pState->camera.GetView() * pState->camera.GetProjection();
        pPerFrame->cameraPos = pState->camera.GetPosition();
        pPerFrame->iblFactor = pState->iblFactor;
        pPerFrame->emmisiveFactor = pState->emmisiveFactor;

        //if the gltf doesn't have any lights set some spotlights
        if (pPerFrame->lightCount == 0)
        {
            pPerFrame->lightCount = pState->spotlightCount;
            for (uint32_t i = 0; i < pState->spotlightCount; i++)
            {
                pPerFrame->lights[i].color[0] = pState->spotlight[i].color.x;
                pPerFrame->lights[i].color[1] = pState->spotlight[i].color.y;
                pPerFrame->lights[i].color[2] = pState->spotlight[i].color.z;

                GetXYZ(pPerFrame->lights[i].position, pState->spotlight[i].light.GetPosition());
                GetXYZ(pPerFrame->lights[i].direction, pState->spotlight[i].light.GetDirection());

                pPerFrame->lights[i].range = 15; //in meters
                pPerFrame->lights[i].type = LightType_Spot;
                pPerFrame->lights[i].intensity = pState->spotlight[i].intensity;
                pPerFrame->lights[i].innerConeCos = cosf(pState->spotlight[i].light.GetFovV()*0.9f/2.0f);
                pPerFrame->lights[i].outerConeCos = cosf(pState->spotlight[i].light.GetFovV() / 2.0f);
                pPerFrame->lights[i].mLightViewProj = pState->spotlight[i].light.GetView() * pState->spotlight[i].light.GetProjection();
            }
        }

        // Up to 4 spotlights can have shadowmaps. Each spot the light has a shadowMap index which is used to find the sadowmap in the atlas
        uint32_t shadowMapIndex = 0;
        for (uint32_t i = 0; i < pPerFrame->lightCount; i++)
        {
            if ((shadowMapIndex < 4) && (pPerFrame->lights[i].type == LightType_Spot))
            {
                pPerFrame->lights[i].shadowMapIndex = shadowMapIndex++; //set the shadowmap index so the color pass knows which shadow map to use
                pPerFrame->lights[i].depthBias = 70.0f / 100000.0f;
            }
        }

        m_pGLTFTexturesAndBuffers->SetPerFrameConstants();

        m_pGLTFTexturesAndBuffers->SetSkinningMatricesForSkeletons();
    }

    // Render to shadow map atlas for spot lights ------------------------------------------
    //
    if (m_pGltfDepth && pPerFrame != NULL)
    {
        SetPerfMarkerBegin(cmd_buf, "ShadowPass");

        VkClearValue depth_clear_values[1];
        depth_clear_values[0].depthStencil.depth = 1.0f;
        depth_clear_values[0].depthStencil.stencil = 0;

        {
            VkRenderPassBeginInfo rp_begin;
            rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.pNext = NULL;
            rp_begin.renderPass = m_render_pass_shadow;
            rp_begin.framebuffer = m_shadowMapBuffers;
            rp_begin.renderArea.offset.x = 0;
            rp_begin.renderArea.offset.y = 0;
            rp_begin.renderArea.extent.width = m_shadowMap.GetWidth();
            rp_begin.renderArea.extent.height = m_shadowMap.GetHeight();
            rp_begin.clearValueCount = 1;
            rp_begin.pClearValues = depth_clear_values;

            vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
            m_GPUTimer.GetTimeStamp(cmd_buf, "Clear Shadow Map");
        }

        uint32_t shadowMapIndex = 0;
        for (uint32_t i = 0; i < pPerFrame->lightCount; i++)
        {
            if (pPerFrame->lights[i].type != LightType_Spot)
                continue;

            // Set the RT's quadrant where to render the shadomap (these viewport offsets need to match the ones in shadowFiltering.h)
            uint32_t viewportOffsetsX[4] = { 0, 1, 0, 1 };
            uint32_t viewportOffsetsY[4] = { 0, 0, 1, 1 };
            uint32_t viewportWidth = m_shadowMap.GetWidth() / 2;
            uint32_t viewportHeight = m_shadowMap.GetHeight() / 2;
            SetViewportAndScissor(cmd_buf, viewportOffsetsX[shadowMapIndex] * viewportWidth, viewportOffsetsY[shadowMapIndex] * viewportHeight, viewportWidth, viewportHeight);

            //set per frame constant buffer values
            GltfDepthPass::per_frame *cbPerFrame = m_pGltfDepth->SetPerFrameConstants();
            cbPerFrame->mViewProj = pPerFrame->lights[i].mLightViewProj;

            m_pGltfDepth->Draw(cmd_buf);

            m_GPUTimer.GetTimeStamp(cmd_buf, "Shadow maps");
            shadowMapIndex++;
        }
        vkCmdEndRenderPass(cmd_buf);
        
        SetPerfMarkerEnd(cmd_buf);
    }

    // Render Scene to the MSAA HDR RT ------------------------------------------------
    //
    SetPerfMarkerBegin(cmd_buf, "Color pass");

    if (pPerFrame != NULL)
    {
        {
            m_GPUTimer.GetTimeStamp(cmd_buf, "before color RP");
            VkClearValue clear_values[2];
            clear_values[0].color.float32[0] = 0.0f;
            clear_values[0].color.float32[1] = 0.0f;
            clear_values[0].color.float32[2] = 0.0f;
            clear_values[0].color.float32[3] = 0.0f;
            clear_values[1].depthStencil.depth = 1.0f;
            clear_values[1].depthStencil.stencil = 0;

            VkRenderPassBeginInfo rp_begin;
            rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.pNext = NULL;
            rp_begin.renderPass = m_render_pass_HDR_MSAA;
            rp_begin.framebuffer = m_frameBuffer_HDR_MSAA;
            rp_begin.renderArea.offset.x = 0;
            rp_begin.renderArea.offset.y = 0;
            rp_begin.renderArea.extent.width = pState->renderWidth;
            rp_begin.renderArea.extent.height = pState->renderHeight;
            rp_begin.clearValueCount = 2;
            rp_begin.pClearValues = clear_values;

            vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdSetScissor(cmd_buf, 0, 1, &m_scissor);
            vkCmdSetViewport(cmd_buf, 0, 1, &m_viewport);
            m_GPUTimer.GetTimeStamp(cmd_buf, "after color RP");
        }

        // Render skydome
        //
        if (pState->skyDomeType == 1)
        {
            SetPerfMarkerBegin(cmd_buf, "Skydome cube");

            XMMATRIX clipToView = XMMatrixInverse(NULL, pPerFrame->mCameraViewProj);
            m_skyDome.Draw(cmd_buf, clipToView);

            m_GPUTimer.GetTimeStamp(cmd_buf, "Skydome cube");
            SetPerfMarkerEnd(cmd_buf);
        }
        else if (pState->skyDomeType == 0)
        {
            SetPerfMarkerBegin(cmd_buf, "Skydome Proc");

            SkyDomeProc::Constants skyDomeConstants;
            skyDomeConstants.invViewProj = XMMatrixInverse(NULL, pPerFrame->mCameraViewProj);
            skyDomeConstants.vSunDirection = XMVectorSet(1.0f, 0.05f, 0.0f, 0.0f);
            skyDomeConstants.turbidity = 10.0f;
            skyDomeConstants.rayleigh = 2.0f;
            skyDomeConstants.mieCoefficient = 0.005f;
            skyDomeConstants.mieDirectionalG = 0.8f;
            skyDomeConstants.luminance = 1.0f;
            skyDomeConstants.sun = false;
            m_skyDomeProc.Draw(cmd_buf, skyDomeConstants);

            m_GPUTimer.GetTimeStamp(cmd_buf, "Skydome Proc");
            SetPerfMarkerEnd(cmd_buf);
        }

        // Render scene to color buffer
        //
        if (m_pGltfBBox && pPerFrame != NULL)
        {
            SetPerfMarkerBegin(cmd_buf, "gltfPBR");

            {
                m_pGltfPBR->Draw(cmd_buf);
                m_GPUTimer.GetTimeStamp(cmd_buf, "Rendering Scene");
            }

            SetPerfMarkerEnd(cmd_buf);
        }

        // draw object's bounding boxes
        //
        if (m_pGltfBBox && pPerFrame != NULL)
        {
            if (pState->bDrawBoundingBoxes)
            {
                SetPerfMarkerBegin(cmd_buf, "bounding boxes");

                m_pGltfBBox->Draw(cmd_buf, pPerFrame->mCameraViewProj);

                m_GPUTimer.GetTimeStamp(cmd_buf, "Bounding Box");

                SetPerfMarkerEnd(cmd_buf);
            }
        }

        // draw light's frustums
        //
        if (pState->bDrawLightFrustum && pPerFrame != NULL)
        {
            SetPerfMarkerBegin(cmd_buf, "light frustrum");

            XMVECTOR vCenter = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
            XMVECTOR vRadius = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
            XMVECTOR vColor = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
            for (uint32_t i = 0; i < pPerFrame->lightCount; i++)
            {
                XMMATRIX spotlightMatrix = XMMatrixInverse(NULL, pPerFrame->lights[i].mLightViewProj);
                XMMATRIX worldMatrix = spotlightMatrix * pPerFrame->mCameraViewProj;
                m_wireframeBox.Draw(cmd_buf, worldMatrix, vCenter, vRadius, vColor);
            }

            m_GPUTimer.GetTimeStamp(cmd_buf, "Light's frustum");

            SetPerfMarkerEnd(cmd_buf);
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    SetPerfMarkerEnd(cmd_buf);

    // Resolve MSAA ------------------------------------------------------------------------
    //
    {
        SetPerfMarkerBegin(cmd_buf, "resolve MSAA");
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.image = m_HDR.Resource();

            vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }

        {
            VkImageResolve re = {};
            re.srcOffset.x = 0;
            re.srcOffset.y = 0;
            re.extent.width = pState->renderWidth;
            re.extent.height = pState->renderHeight;
            re.extent.depth = 1;
            re.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            re.srcSubresource.layerCount = 1;
            re.dstOffset.x = 0;
            re.dstOffset.y = 0;
            re.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            re.dstSubresource.layerCount = 1;
            vkCmdResolveImage(cmd_buf, m_HDRMSAA.Resource(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_HDR.Resource(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &re);
        }

        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // we need to read from it for the post-processing
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.image = m_HDR.Resource();

            vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        }

        m_GPUTimer.GetTimeStamp(cmd_buf, "Resolve");
        SetPerfMarkerEnd(cmd_buf);
    }

    // Post proc---------------------------------------------------------------------------
    //
    {
        SetPerfMarkerBegin(cmd_buf, "post proc");
        
        // Downsample pass
        SetPerfMarkerBegin(cmd_buf, "Downsample");
        m_downSample.Draw(cmd_buf);

        //m_downSample.Gui();
        m_GPUTimer.GetTimeStamp(cmd_buf, "Downsample");
        SetPerfMarkerEnd(cmd_buf);

        // Bloom pass (needs the downsampled data)
        SetPerfMarkerBegin(cmd_buf, "bloom");
        m_bloom.Draw(cmd_buf);
        //m_bloom.Gui();
        m_GPUTimer.GetTimeStamp(cmd_buf, "bloom");
        SetPerfMarkerEnd(cmd_buf);

        SetPerfMarkerEnd(cmd_buf);
    }

    int imageIndex = pSwapChain->WaitForSwapChain();
    SetPerfMarkerBegin(cmd_buf, "rendering to swap chain");

    // Tonemapping ------------------------------------------------------------------------
    //
    {
        SetPerfMarkerBegin(cmd_buf, "tonemapping");

        // prepare render pass
        {
            VkRenderPassBeginInfo rp_begin = {};
            rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.pNext = NULL;
            rp_begin.renderPass = m_render_pass_tonemap;
            rp_begin.framebuffer = m_frameBuffer_tonemap;
            rp_begin.renderArea.offset.x = 0;
            rp_begin.renderArea.offset.y = 0;
            rp_begin.renderArea.extent.width = pState->renderWidth;
            rp_begin.renderArea.extent.height = pState->renderHeight;
            rp_begin.clearValueCount = 0;
            rp_begin.pClearValues = NULL;
            vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
        }

        vkCmdSetScissor(cmd_buf, 0, 1, &m_scissor);
        vkCmdSetViewport(cmd_buf, 0, 1, &m_viewport);

        m_toneMapping.Draw(cmd_buf, pState->exposure, pState->toneMapper);
        vkCmdEndRenderPass(cmd_buf);

        m_GPUTimer.GetTimeStamp(cmd_buf, "Tone mapping");

        SetPerfMarkerEnd(cmd_buf);
    }

    // Cas ------------------------------------------------------------------------
    //
    {
        m_CAS.Upscale(cmd_buf, m_tonemapTexture, m_tonemapSRV, pState->CASState != CAS_State_NoCas, pState->usePackedMath, pState->CASState);
        m_GPUTimer.GetTimeStamp(cmd_buf, "CAS");
        SetPerfMarkerEnd(cmd_buf);

        // prepare render pass
        {
            VkRenderPassBeginInfo rp_begin = {};
            rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin.pNext = NULL;
            rp_begin.renderPass = m_render_pass_swap_chain;
            rp_begin.framebuffer = pSwapChain->GetFramebuffer(imageIndex);
            rp_begin.renderArea.offset.x = 0;
            rp_begin.renderArea.offset.y = 0;
            rp_begin.renderArea.extent.width = m_Width;
            rp_begin.renderArea.extent.height = m_Height;
            rp_begin.clearValueCount = 0;
            rp_begin.pClearValues = NULL;
            vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
        }

        vkCmdSetScissor(cmd_buf, 0, 1, &m_finalScissor);
        vkCmdSetViewport(cmd_buf, 0, 1, &m_finalViewport);

        m_CAS.DrawToSwapChain(cmd_buf, m_tonemapSRV, pState->CASState != CAS_State_NoCas);
    }

    // Render HUD  ------------------------------------------------------------------------
    //
    {
        SetPerfMarkerBegin(cmd_buf, "ImGUI");
        m_ImGUI.Draw(cmd_buf);
        m_GPUTimer.GetTimeStamp(cmd_buf, "ImGUI Rendering");
        SetPerfMarkerEnd(cmd_buf);
    }

    SetPerfMarkerEnd(cmd_buf);

    m_GPUTimer.OnEndFrame();

    vkCmdEndRenderPass(cmd_buf);

    VkResult res = vkEndCommandBuffer(cmd_buf);
    assert(res == VK_SUCCESS);
    
    // Close & Submit the command list ----------------------------------------------------
    //
    VkSemaphore ImageAvailableSemaphore;
    VkSemaphore RenderFinishedSemaphores;
    VkFence CmdBufExecutedFences;
    pSwapChain->GetSemaphores(&ImageAvailableSemaphore, &RenderFinishedSemaphores, &CmdBufExecutedFences);

    VkPipelineStageFlags submitWaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkCommandBuffer cmd_bufs[] = { cmd_buf };
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &ImageAvailableSemaphore;
    submit_info.pWaitDstStageMask = &submitWaitStage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = cmd_bufs;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &RenderFinishedSemaphores;

    res = vkQueueSubmit(m_pDevice->GetGraphicsQueue(), 1, &submit_info, CmdBufExecutedFences);
    assert(res == VK_SUCCESS);
}

void CAS_Renderer::UpdateCASSharpness(float NewSharpenVal, CAS_State CasState)
{
    m_CAS.UpdateSharpness(NewSharpenVal, CasState);
}
