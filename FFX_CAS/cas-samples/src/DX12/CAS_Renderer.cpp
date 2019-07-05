#//CAS Sample
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
void CAS_Renderer::OnCreate(Device* pDevice, SwapChain *pSwapChain)
{
    m_pDevice = pDevice;

    // Initialize helpers

    // Create all the heaps for the resources views
    m_resourceViewHeaps.OnCreate(pDevice, 2000, 4000, 10, 3, 60, 2048);

    // Create a commandlist ring for the Direct queue
    m_CommandListRing.OnCreate(pDevice, cNumSwapBufs, 8, pDevice->GetGraphicsQueue()->GetDesc());

    // Create a 'dynamic' constant buffers ring
    m_ConstantBufferRing.OnCreate(pDevice, cNumSwapBufs, 2 * 1024 * 1024, &m_resourceViewHeaps);

    // Create a 'static' constant buffer pool
    m_VidMemBufferPool.OnCreate(pDevice, 128 * 1024 * 1024, USE_VID_MEM, "StaticGeom");

    // initialize the GPU time stamps module
    m_GPUTimer.OnCreate(pDevice, cNumSwapBufs);

    // Quick helper to upload resources, it has it's own commandList and uses suballocation.
    // for 4K textures we'll need 100Megs
    m_UploadHeap.OnCreate(pDevice, 100 * 1024 * 1024);    // initialize an upload heap (uses suballocation for faster results)

    // Create the depth buffer view
    m_resourceViewHeaps.AllocDSVDescriptor(1, &m_depthBufferDSV);

    // Create a Shadowmap atlas to hold 4 cascades/spotlights
    m_shadowMap.InitDepthStencil(pDevice, "m_pShadowMap", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, 2*1024, 2*1024, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL));
    m_resourceViewHeaps.AllocDSVDescriptor(1, &m_ShadowMapDSV);
    m_shadowMap.CreateDSV(0, &m_ShadowMapDSV);

    m_skyDome.OnCreate(pDevice, &m_UploadHeap, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, "..\\..\\cauldron-media\\envmaps\\papermill\\diffuse.dds", "..\\..\\cauldron-media\\envmaps\\papermill\\specular.dds", DXGI_FORMAT_R16G16B16A16_FLOAT, 4);
    m_skyDomeProc.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT, 4);
    m_wireframeBox.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT, 4);
    m_downSample.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_bloom.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);

    // Create tonemapping pass
    m_toneMapping.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, DXGI_FORMAT_R16G16B16A16_FLOAT);

    // Create Cas passes
    m_CAS.OnCreate(pDevice, &m_resourceViewHeaps, &m_ConstantBufferRing, &m_VidMemBufferPool, pSwapChain->GetFormat());

    // Initialize UI rendering resources
    m_ImGUI.OnCreate(pDevice, &m_UploadHeap, &m_resourceViewHeaps, &m_ConstantBufferRing, pSwapChain->GetFormat());

    m_resourceViewHeaps.AllocRTVDescriptor(1, &m_HDRRTV);
    m_resourceViewHeaps.AllocRTVDescriptor(1, &m_HDRRTVMSAA);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_HDRSRV);

    m_resourceViewHeaps.AllocRTVDescriptor(1, &m_TonemapRTV);
    m_resourceViewHeaps.AllocCBV_SRV_UAVDescriptor(1, &m_TonemapSRV);

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
    m_CAS.OnDestroy();
    m_toneMapping.OnDestroy();
    m_ImGUI.OnDestroy();
    m_bloom.OnDestroy();
    m_downSample.OnDestroy();
    m_wireframeBox.OnDestroy();
    m_skyDomeProc.OnDestroy();
    m_skyDome.OnDestroy();
    m_shadowMap.OnDestroy();

    m_UploadHeap.OnDestroy();
    m_GPUTimer.OnDestroy();
    m_VidMemBufferPool.OnDestroy();
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
    m_Width = Width;
    m_Height = Height;

    int targetWidth = Width;
    int targetHeight = Height;
    if (pState->CASState == CAS_State_SharpenOnly)
    {
        targetWidth = pState->renderWidth;
        targetHeight = pState->renderHeight;
    }

    // Set the viewport
    //
    m_ViewPort = { 0.0f, 0.0f, static_cast<float>(pState->renderWidth), static_cast<float>(pState->renderHeight), 0.0f, 1.0f };
    m_FinalViewPort = { 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };

    // Create scissor rectangle
    //
    m_RectScissor = { 0, 0, static_cast<LONG>(pState->renderWidth), static_cast<LONG>(pState->renderHeight) };
    m_FinalRectScissor = { 0, 0, static_cast<LONG>(Width), static_cast<LONG>(Height) };

    // Create depth buffer    
    //
    m_depthBuffer.InitDepthStencil(m_pDevice, "depthbuffer", &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, pState->renderWidth, pState->renderHeight, 1, 1, 4, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE));
    m_depthBuffer.CreateDSV(0, &m_depthBufferDSV);

    // Create Texture + RTV with x4 MSAA
    //
    CD3DX12_RESOURCE_DESC RDescMSAA = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, pState->renderWidth, pState->renderHeight, 1, 1, 4, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    m_HDRMSAA.InitRenderTarget(m_pDevice, "HDRMSAA", &RDescMSAA, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_HDRMSAA.CreateRTV(0, &m_HDRRTVMSAA);

    // Create Texture + RTV, to hold the resolved scene 
    //
    CD3DX12_RESOURCE_DESC RDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, pState->renderWidth, pState->renderHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    m_HDR.InitRenderTarget(m_pDevice, "HDR", &RDesc, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_HDR.CreateSRV(0, &m_HDRSRV);
    m_HDR.CreateRTV(0, &m_HDRRTV);   
    
    // Create Texture + RTV to hold tone mapped image
    CD3DX12_RESOURCE_DESC TDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, pState->renderWidth, pState->renderHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    m_Tonemap.InitRenderTarget(m_pDevice, "Tonemap", &TDesc, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_Tonemap.CreateSRV(0, &m_TonemapSRV);
    m_Tonemap.CreateRTV(0, &m_TonemapRTV);

    // Update bloom, downscaling and CAS effects
    m_downSample.OnCreateWindowSizeDependentResources(pState->renderWidth, pState->renderHeight, &m_HDR, 5); //downsample the HDR texture 5 times
    m_bloom.OnCreateWindowSizeDependentResources(pState->renderWidth /2 , pState->renderHeight / 2, m_downSample.GetTexture(), 5, &m_HDR);
    m_CAS.OnCreateWindowSizeDependentResources(m_pDevice, pState->renderWidth, pState->renderHeight, targetWidth, targetHeight, pState->CASState, pState->usePackedMath);
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
            &m_UploadHeap,
            &m_resourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers
        );
    }
    else if (stage == 8)
    {
        Profile p("m_gltfPBR->OnCreate");
        
        // same thing as above but for the PBR pass
        m_pGltfPBR = new GltfPbrPass();
        m_pGltfPBR->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_resourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            &m_skyDome,
            &m_shadowMap,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            4
        );
    }
    else if (stage == 9)
    {
        Profile p("m_gltfBBox->OnCreate");

        // just a bounding box pass that will draw boundingboxes instead of the geometry itself
        m_pGltfBBox = new GltfBBoxPass();
        m_pGltfBBox->OnCreate(
            m_pDevice,
            &m_UploadHeap,
            &m_resourceViewHeaps,
            &m_ConstantBufferRing,
            &m_VidMemBufferPool,
            m_pGLTFTexturesAndBuffers,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            4
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
    // Timing values
    //
    UINT64 gpuTicksPerSecond;
    m_pDevice->GetGraphicsQueue()->GetTimestampFrequency(&gpuTicksPerSecond);
    
    // Let our resource managers do some house keeping 
    //
    m_ConstantBufferRing.OnBeginFrame();
    m_GPUTimer.OnBeginFrame(gpuTicksPerSecond, &m_TimeStamps);
    
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
                pPerFrame->lights[i].innerConeCos = cosf(pState->spotlight[i].light.GetFovV()*0.9f / 2.0f);
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

    // command buffer calls
    //    
    ID3D12GraphicsCommandList* pCmdLst1 = m_CommandListRing.GetNewCommandList();

    m_GPUTimer.GetTimeStamp(pCmdLst1, "Begin Frame");

    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clears -----------------------------------------------------------------------
    //
    pCmdLst1->ClearDepthStencilView(m_ShadowMapDSV.GetCPU(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_GPUTimer.GetTimeStamp(pCmdLst1, "Clear Shadow Map");

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    pCmdLst1->ClearRenderTargetView(m_HDRRTVMSAA.GetCPU(), clearColor, 0, nullptr);
    m_GPUTimer.GetTimeStamp(pCmdLst1, "Clear HDR");

    pCmdLst1->ClearDepthStencilView(m_depthBufferDSV.GetCPU(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_GPUTimer.GetTimeStamp(pCmdLst1, "Clear Depth");

    // Render to shadow map atlas for spot lights ------------------------------------------
    //
    if (m_pGltfDepth && pPerFrame != NULL)
    {        
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
            SetViewportAndScissor(pCmdLst1, viewportOffsetsX[i] * viewportWidth, viewportOffsetsY[i] * viewportHeight, viewportWidth, viewportHeight);
            pCmdLst1->OMSetRenderTargets(0, NULL, true, &m_ShadowMapDSV.GetCPU());

            GltfDepthPass::per_frame *cbDepthPerFrame = m_pGltfDepth->SetPerFrameConstants();
            cbDepthPerFrame->mViewProj = pPerFrame->lights[i].mLightViewProj;

            m_pGltfDepth->Draw(pCmdLst1);
            
            m_GPUTimer.GetTimeStamp(pCmdLst1, "Shadow map");
            shadowMapIndex++;
        }        
    }

    // Render Scene to the MSAA HDR RT ------------------------------------------------
    //
    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    pCmdLst1->RSSetViewports(1, &m_ViewPort);
    pCmdLst1->RSSetScissorRects(1, &m_RectScissor);
    pCmdLst1->OMSetRenderTargets(1, &m_HDRRTVMSAA.GetCPU(), true, &m_depthBufferDSV.GetCPU());

    if (pPerFrame != NULL)
    {
        // Render skydome
        //
        if (pState->skyDomeType == 1)
        {
            XMMATRIX clipToView = XMMatrixInverse(NULL, pPerFrame->mCameraViewProj);
            m_skyDome.Draw(pCmdLst1, clipToView);

            m_GPUTimer.GetTimeStamp(pCmdLst1, "Skydome");
        }
        else if (pState->skyDomeType == 0)
        {
            SkyDomeProc::Constants skyDomeConstants;
            skyDomeConstants.invViewProj = XMMatrixInverse(NULL, pPerFrame->mCameraViewProj);
            skyDomeConstants.vSunDirection = XMVectorSet(1.0f, 0.05f, 0.0f, 0.0f);
            skyDomeConstants.turbidity = 10.0f;
            skyDomeConstants.rayleigh = 2.0f;
            skyDomeConstants.mieCoefficient = 0.005f;
            skyDomeConstants.mieDirectionalG = 0.8f;
            skyDomeConstants.luminance = 1.0f;
            skyDomeConstants.sun = false;
            m_skyDomeProc.Draw(pCmdLst1, skyDomeConstants);

            m_GPUTimer.GetTimeStamp(pCmdLst1, "Skydome Proc");
        }

        // Render scene to color buffer
        //    
        if (m_pGltfPBR && pPerFrame != NULL)
        {
            //set per frame constant buffer values
            m_pGltfPBR->Draw(pCmdLst1);
        }

        // draw object's bounding boxes
        //
        if (m_pGltfBBox && pPerFrame != NULL)
        {
            if (pState->bDrawBoundingBoxes)
            {
                m_pGltfBBox->Draw(pCmdLst1, pPerFrame->mCameraViewProj);
                m_GPUTimer.GetTimeStamp(pCmdLst1, "Bounding Box");
            }
        }

        // draw light's frustums
        //
        if (pState->bDrawLightFrustum && pPerFrame != NULL)
        {
            //SetPerfMarkerBegin(cmd_buf, "light frustrum");

            XMVECTOR vCenter = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
            XMVECTOR vRadius = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
            XMVECTOR vColor  = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
            for (uint32_t i = 0; i < pPerFrame->lightCount; i++)
            {
                XMMATRIX spotlightMatrix = XMMatrixInverse(NULL, pPerFrame->lights[i].mLightViewProj);
                XMMATRIX worldMatrix = spotlightMatrix * pPerFrame->mCameraViewProj;
                m_wireframeBox.Draw(pCmdLst1, worldMatrix, vCenter, vRadius, vColor);
            }

            m_GPUTimer.GetTimeStamp(pCmdLst1, "Light's frustum");

            //SetPerfMarkerEnd(cmd_buf);
        }
    }
    pCmdLst1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_shadowMap.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    m_GPUTimer.GetTimeStamp(pCmdLst1, "Rendering Scene");

    // Resolve MSAA ------------------------------------------------------------------------
    //
    {
        D3D12_RESOURCE_BARRIER Barriers[] = 
        {
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_DEST),
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDRMSAA.GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
        };
        pCmdLst1->ResourceBarrier(_countof(Barriers), Barriers);
    }

    pCmdLst1->ResolveSubresource(m_HDR.GetResource(), 0, m_HDRMSAA.GetResource(), 0, DXGI_FORMAT_R16G16B16A16_FLOAT);

    {
        D3D12_RESOURCE_BARRIER Barriers[] = 
        {
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_HDRMSAA.GetResource(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
        };
        pCmdLst1->ResourceBarrier(_countof(Barriers), Barriers); // the bloom needs to read from this
    }

    m_GPUTimer.GetTimeStamp(pCmdLst1, "Resolve");

    // Post proc---------------------------------------------------------------------------
    //
    {
        m_downSample.Draw(pCmdLst1);
        //m_downSample.Gui();
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Downsample");

        m_bloom.Draw(pCmdLst1, &m_HDR);
        //m_bloom.Gui();
        m_GPUTimer.GetTimeStamp(pCmdLst1, "Bloom");
    }

    ThrowIfFailed(pCmdLst1->Close());
    ID3D12CommandList* CmdListList1[] = { pCmdLst1 };
    m_pDevice->GetGraphicsQueue()->ExecuteCommandLists(1, CmdListList1);

    // Wait for swapchain (we are going to render to it) -----------------------------------
    //
    pSwapChain->WaitForSwapChain();
    m_CommandListRing.OnBeginFrame();

    ID3D12GraphicsCommandList* pCmdLst2 = m_CommandListRing.GetNewCommandList();

    // Tonemapping ------------------------------------------------------------------------
    //
    {
        pCmdLst2->RSSetViewports(1, &m_ViewPort);
        pCmdLst2->RSSetScissorRects(1, &m_RectScissor);
        pCmdLst2->OMSetRenderTargets(1, &m_TonemapRTV.GetCPU(), true, NULL);

        m_toneMapping.Draw(pCmdLst2, &m_HDRSRV, pState->exposure, pState->toneMapper);
        m_GPUTimer.GetTimeStamp(pCmdLst2, "Tone mapping");

        pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_HDR.GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
    }

    // Cas ------------------------------------------------------------------------
    //
    {
        pCmdLst2->RSSetViewports(1, &m_FinalViewPort);
        pCmdLst2->RSSetScissorRects(1, &m_FinalRectScissor);
        pCmdLst2->OMSetRenderTargets(1, pSwapChain->GetCurrentBackBufferRTV(), true, NULL);

        m_CAS.Upscale(pCmdLst2, pState->CASState != CAS_State_NoCas, pState->usePackedMath, (CAS_State)pState->CASState, m_Tonemap.GetResource(), m_TonemapSRV);

        m_GPUTimer.GetTimeStamp(pCmdLst2, "CAS");

        m_CAS.Draw(pCmdLst2, pState->CASState != CAS_State_NoCas, m_Tonemap.GetResource(), m_TonemapSRV);
    }

    // Render HUD  ------------------------------------------------------------------------
    //
    {
        pCmdLst2->RSSetViewports(1, &m_FinalViewPort);
        pCmdLst2->RSSetScissorRects(1, &m_FinalRectScissor);
        pCmdLst2->OMSetRenderTargets(1, pSwapChain->GetCurrentBackBufferRTV(), true, NULL);

        m_ImGUI.Draw(pCmdLst2);

        m_GPUTimer.GetTimeStamp(pCmdLst2, "ImGUI Rendering");
    }
    
    // Transition swapchain into present mode

    pCmdLst2->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pSwapChain->GetCurrentBackBufferResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    m_GPUTimer.OnEndFrame();

    m_GPUTimer.CollectTimings(pCmdLst2);

    // Close & Submit the command list ----------------------------------------------------
    //
    ThrowIfFailed(pCmdLst2->Close());

    ID3D12CommandList* CmdListList2[] = { pCmdLst2 };
    m_pDevice->GetGraphicsQueue()->ExecuteCommandLists(1, CmdListList2);
    m_pDevice->GPUFlush();
}

void CAS_Renderer::UpdateCASSharpness(float NewSharpenVal, CAS_State CasState)
{
    m_CAS.UpdateSharpness(NewSharpenVal, CasState);
}
