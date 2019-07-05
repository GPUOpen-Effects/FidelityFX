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
#include "CAS_CS.h"

// CAS
#define A_CPU
#include "ffx_a.h"
#include "ffx_cas.h"

namespace CAS_SAMPLE_DX12
{
    void CAS_Filter::OnCreate(
        Device *pDevice,
        ResourceViewHeaps      *pResourceViewHeaps,
        DynamicBufferRing      *pConstantBufferRing,
        StaticBufferPool       *pStaticBufferPool,
        DXGI_FORMAT             outFormat
    )
    {
        m_pResourceViewHeaps = pResourceViewHeaps;
        m_pConstantBufferRing = pConstantBufferRing;
        m_outFormat = outFormat;
        m_pDevice = pDevice;
        
        m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_constBuffer);
        m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputTextureUav);
        m_pResourceViewHeaps->AllocCBV_SRV_UAVDescriptor(1, &m_outputTextureSrv);
        
        D3D12_STATIC_SAMPLER_DESC SamplerDesc = {};
        SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        SamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        SamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        SamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        SamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        SamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        SamplerDesc.MinLOD = 0.0f;
        SamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        SamplerDesc.MipLODBias = 0;
        SamplerDesc.MaxAnisotropy = 1;
        SamplerDesc.ShaderRegister = 0;
        SamplerDesc.RegisterSpace = 0;
        SamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        DefineList defines;
        defines["CAS_SAMPLE_FP16"] = "0";
        defines["CAS_SAMPLE_SHARPEN_ONLY"] = "0";

        if (pDevice->IsFp16Supported())
        {
            defines["CAS_SAMPLE_FP16"] = "1";

            defines["CAS_SAMPLE_SHARPEN_ONLY"] = "1"; 
            m_casPackedSharpenOnly.OnCreate(pDevice, pResourceViewHeaps, "CAS_Shader.hlsl", "mainCS", 1, 1, 64, 1, 1, &defines);

            defines["CAS_SAMPLE_SHARPEN_ONLY"] = "0";
            m_casPackedFast.OnCreate(pDevice, pResourceViewHeaps, "CAS_Shader.hlsl", "mainCS", 1, 1, 64, 1, 1, &defines);
        }

        defines["CAS_SAMPLE_FP16"] = "0";

        defines["CAS_SAMPLE_SHARPEN_ONLY"] = "1";
        m_casSharpenOnly.OnCreate(pDevice, pResourceViewHeaps, "CAS_Shader.hlsl", "mainCS", 1, 1, 64, 1, 1, &defines);

        defines["CAS_SAMPLE_SHARPEN_ONLY"] = "0"; 
        m_casFast.OnCreate(pDevice, pResourceViewHeaps, "CAS_Shader.hlsl", "mainCS", 1, 1, 64, 1, 1, &defines);

        m_renderFullscreen.OnCreate(pDevice, "CAS_RenderPS.hlsl", pResourceViewHeaps, pStaticBufferPool, 1, &SamplerDesc, outFormat);
    }

    void CAS_Filter::OnCreateWindowSizeDependentResources(Device *pDevice, uint32_t renderWidth, uint32_t renderHeight, uint32_t Width, uint32_t Height, CAS_State CASState, bool packedMathEnabled)
    {
        m_renderWidth = renderWidth;
        m_renderHeight = renderHeight;
        m_width = Width;
        m_height = Height;

        // Create lower res texture that we will upsample
        {
            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            textureDesc.Width = Width;
            textureDesc.Height = Height;
            textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            ThrowIfFailed(pDevice->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_pOutputTexture)));

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Texture2D.PlaneSlice = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0;
            m_pDevice->GetDevice()->CreateShaderResourceView(m_pOutputTexture, &srvDesc, m_outputTextureSrv.GetCPU());

            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = 0;
            uavDesc.Texture2D.PlaneSlice = 0;

            pDevice->GetDevice()->CreateUnorderedAccessView(m_pOutputTexture, 0, &uavDesc, m_outputTextureUav.GetCPU());
            
            UpdateSharpness(m_sharpenVal, CASState);
        }
    }

    void CAS_Filter::OnDestroyWindowSizeDependentResources()
    {
        if (m_pOutputTexture)
        {
            m_pOutputTexture->Release();
        }
    }

    void CAS_Filter::OnDestroy()
    {
    }

    void CAS_Filter::Upscale(ID3D12GraphicsCommandList* pCommandList, bool useCas, bool usePacked, CAS_State casState, ID3D12Resource* inputResource, CBV_SRV_UAV inputSrv)
    {
        D3D12_GPU_VIRTUAL_ADDRESS cbHandle = {};
        uint32_t* pConstMem = 0;
        m_pConstantBufferRing->AllocConstantBuffer(sizeof(CASConstants), (void **)&pConstMem, &cbHandle);
        memcpy(pConstMem, &m_consts, sizeof(CASConstants));

        // This value is the image region dim that each thread group of the CAS shader operates on
        static const int threadGroupWorkRegionDim = 16;
        int dispatchX = (m_width + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
        int dispatchY = (m_height + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;

        if (useCas)
        {
            pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(inputResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0));

            if (usePacked)
            {
                if (casState == CAS_State_SharpenOnly)
                {
                    m_casPackedSharpenOnly.Draw(pCommandList, cbHandle, &m_outputTextureUav, &inputSrv, dispatchX, dispatchY, 1);
                }
                else if (casState == CAS_State_Upsample)
                {
                    m_casPackedFast.Draw(pCommandList, cbHandle, &m_outputTextureUav, &inputSrv, dispatchX, dispatchY, 1);
                }
            }
            else
            {
                if (casState == CAS_State_SharpenOnly)
                {
                    m_casSharpenOnly.Draw(pCommandList, cbHandle, &m_outputTextureUav, &inputSrv, dispatchX, dispatchY, 1);
                }
                else if (casState == CAS_State_Upsample)
                {
                    m_casFast.Draw(pCommandList, cbHandle, &m_outputTextureUav, &inputSrv, dispatchX, dispatchY, 1);
                }
            }

            // Transition our UAV to display it
            D3D12_RESOURCE_BARRIER Barriers[] = 
            {
                CD3DX12_RESOURCE_BARRIER::Transition(m_pOutputTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0),
                CD3DX12_RESOURCE_BARRIER::Transition(inputResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, 0),
            };
            pCommandList->ResourceBarrier(_countof(Barriers), Barriers);
        }
    }

    void CAS_Filter::Draw(ID3D12GraphicsCommandList* pCommandList, bool useCas, ID3D12Resource* inputResource, CBV_SRV_UAV inputSrv)
    {
        // NOTE: Render to swap chain
        if (useCas)
        {
            m_renderFullscreen.Draw(pCommandList, 1, &m_outputTextureSrv, 0);
            pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_pOutputTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0));
        }
        else
        {
            pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(inputResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0));
            m_renderFullscreen.Draw(pCommandList, 1, &inputSrv, 0);
            pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(inputResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, 0));
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
