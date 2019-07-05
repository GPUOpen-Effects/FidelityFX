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

#include "CAS_Sample.h"

#if _DEBUG
const bool VALIDATION_ENABLED = true;
#else
const bool VALIDATION_ENABLED = false;
#endif

CAS_Sample::CAS_Sample(LPCSTR name) : FrameworkWindows(name)
{
    m_lastFrameTime = MillisecondsNow();
    m_time = 0;
    m_bPlay = true;

    m_pGltfLoader = NULL;
}

//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void CAS_Sample::OnCreate(HWND hWnd)
{
    // Create Device
    //
    m_device.OnCreate("CAS DX12 Sample v1.0", "CAS DX12 Engine v1.0", VALIDATION_ENABLED, hWnd);
    m_device.CreatePipelineCache();

    //init the shader compiler
    CreateShaderCache();

    // Create Swapchain
    //
    m_swapChain.OnCreate(&m_device, cNumSwapBufs, hWnd);

    // Create a instance of the renderer and initialize it, we need to do that for each GPU
    //
    m_pNode = new CAS_Renderer();
    m_pNode->OnCreate(&m_device, &m_swapChain);

    // init GUI (non gfx stuff)
    //
    ImGUI_Init((void *)hWnd);

    // Init Camera, looking at the origin
    //
    m_roll = 0.0f;
    m_pitch = 0.0f;
    m_distance = 3.5f;

    // init GUI state   
    m_state.toneMapper = 0;
    m_state.skyDomeType = 0;
    m_state.exposure = 1.0f;
    m_state.iblFactor = 2.0f;
    m_state.emmisiveFactor = 1.0f;
    m_state.bDrawLightFrustum = false;
    m_state.bDrawBoundingBoxes = false;
    m_state.camera.LookAt(m_roll, m_pitch, m_distance, XMVectorSet(0, 0, 0, 0));
    
    // NOTE: Init render width/height and display mode
    m_state.usePackedMath = false;
    m_state.CASState = CAS_State_NoCas;
    m_currDisplayMode = 0;
    m_state.renderWidth = 0;
    m_state.renderHeight = 0;
    m_state.sharpenControl = 0.0f;
    m_state.profiling = false;

    m_state.spotlightCount = 1;

    m_state.spotlight[0].intensity = 10.0f;
    m_state.spotlight[0].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
    m_state.spotlight[0].light.SetFov(XM_PI / 2.0f, 1024, 1024, 0.1f, 100.0f);
    m_state.spotlight[0].light.LookAt(XM_PI / 2.0f, 0.58f, 3.5f, XMVectorSet(0, 0, 0, 0));

    // Init profiling state
    m_CASTimingsCurrId = 0;
    m_CASAvgTiming = 0;
}

//--------------------------------------------------------------------------------------
//
// OnDestroy
//
//--------------------------------------------------------------------------------------
void CAS_Sample::OnDestroy()
{
    ImGUI_Shutdown();
    
    m_device.GPUFlush();

    // Fullscreen state should always be false before exiting the app.
    m_swapChain.SetFullScreen(false);

    m_pNode->UnloadScene();
    m_pNode->OnDestroyWindowSizeDependentResources();
    m_pNode->OnDestroy();

    if (m_pNode)
    {
        delete m_pNode;
        m_pNode = 0;
    }

    m_swapChain.OnDestroyWindowSizeDependentResources();
    m_swapChain.OnDestroy();

    //shut down the shader compiler 
    DestroyShaderCache(&m_device);

    if (m_pGltfLoader)
    {
        delete m_pGltfLoader;
        m_pGltfLoader = NULL;
    }

    m_device.OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// OnEvent
//
//--------------------------------------------------------------------------------------
bool CAS_Sample::OnEvent(MSG msg)
{
    if (ImGUI_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam))
        return true;

    return true;
}

//--------------------------------------------------------------------------------------
//
// SetFullScreen
//
//--------------------------------------------------------------------------------------
void CAS_Sample::SetFullScreen(bool fullscreen)
{
    m_device.GPUFlush();

    m_swapChain.SetFullScreen(fullscreen);    
}

//--------------------------------------------------------------------------------------
//
// OnResize
//
//--------------------------------------------------------------------------------------
void CAS_Sample::OnResize(uint32_t width, uint32_t height)
{
    if (m_Width != width || m_Height != height)
    {
        // Flush GPU
        //
        m_device.GPUFlush();

        // If resizing but no minimizing
        //
        if (m_Width > 0 && m_Height > 0)
        {
            m_pNode->OnDestroyWindowSizeDependentResources();
            m_swapChain.OnDestroyWindowSizeDependentResources();
        }

        m_Width = width;
        m_Height = height;
        
        // NOTE: Reset render width/height and display mode
        {
            std::vector<ResolutionInfo> supportedResolutions = {};
            CAS_Filter::GetSupportedResolutions(m_Width, m_Height, supportedResolutions);

            m_currDisplayMode = 0;
            m_state.renderWidth = supportedResolutions[m_currDisplayMode].Width;
            m_state.renderHeight = supportedResolutions[m_currDisplayMode].Height;
        }

        // if resizing but not minimizing the recreate it with the new size
        //
        if (m_Width > 0 && m_Height > 0)
        {
            m_swapChain.OnCreateWindowSizeDependentResources(m_Width, m_Height);
            m_pNode->OnCreateWindowSizeDependentResources(&m_swapChain, &m_state, m_Width, m_Height);
        }
    }
    m_state.camera.SetFov(XM_PI / 4, m_Width, m_Height, 0.1f, 1000.0f);
}

//--------------------------------------------------------------------------------------
//
// OnRender
//
//--------------------------------------------------------------------------------------
void CAS_Sample::OnRender()
{
    // Get timings
    //
    double timeNow = MillisecondsNow();
    m_deltaTime = timeNow - m_lastFrameTime;
    m_lastFrameTime = timeNow;

    // Build UI and set the scene state. Note that the rendering of the UI happens later.
    //    
    ImGUI_UpdateIO();
    ImGui::NewFrame();

    static int loadingStage = 0;
    if (loadingStage >= 0)
    {
        // LoadScene needs to be called a number of times, the scene is not fully loaded until it returns -1
        // This is done so we can display a progress bar when the scene is loading
        if (m_pGltfLoader == NULL)
        {
            m_pGltfLoader = new GLTFCommon();
            m_pGltfLoader->Load("..\\..\\cauldron-media\\DamagedHelmet\\glTF\\", "DamagedHelmet.gltf");
            loadingStage = 0;
        }
        loadingStage = m_pNode->LoadScene(m_pGltfLoader, loadingStage);
    }
    else
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameBorderSize = 1.0f;

        bool opened = true;
        ImGui::Begin("Stats", &opened);

        if (ImGui::CollapsingHeader("Info", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Resolution       : %ix%i", m_Width, m_Height);
        }

        if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Play", &m_bPlay);
            ImGui::SliderFloat("Time", &m_time, 0, 30);
        }
      
        if (ImGui::CollapsingHeader("Model Selection", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const char * models[] = { "busterDrone", "BoomBox", "SciFiHelmet", "DamagedHelmet", "Sponza", "MetalRoughSpheres" };
            static int selected = 3;
            if (ImGui::Combo("model", &selected, models, _countof(models)))
            {
                {
                    //free resources, unload the current scene, and load new scene...
                    m_device.GPUFlush();

                    m_pNode->UnloadScene();
                    m_pNode->OnDestroyWindowSizeDependentResources();
                    m_pGltfLoader->Unload();
                    m_pNode->OnDestroy();
                    m_pNode->OnCreate(&m_device, &m_swapChain);
                    m_pNode->OnCreateWindowSizeDependentResources(&m_swapChain, &m_state, m_Width, m_Height);
                }

                m_pGltfLoader = new GLTFCommon();
                bool res = false;
                switch (selected)
                {
                case 0: 
                    m_state.iblFactor = 1.0f;
                    m_roll = 0.0f; m_pitch = 0.0f; m_distance = 3.5f;
                    m_state.camera.LookAt(m_roll, m_pitch, m_distance, XMVectorSet(0, 0, 0, 0));
                    res = m_pGltfLoader->Load("..\\..\\cauldron-media\\buster_drone\\", "busterDrone.gltf"); break;
                case 1: 
                    m_state.iblFactor = 1.0f;
                    m_roll = 0.0f; m_pitch = 0.0f; m_distance = 3.5f;
                    m_state.camera.LookAt(m_roll, m_pitch, m_distance, XMVectorSet(0, 0, 0, 0));
                    res = m_pGltfLoader->Load("..\\..\\cauldron-media\\BoomBox\\glTF\\", "BoomBox.gltf"); break;
                case 2: 
                    m_state.iblFactor = 1.0f;
                    m_roll = 0.0f; m_pitch = 0.0f; m_distance = 3.5f;
                    m_state.camera.LookAt(m_roll, m_pitch, m_distance, XMVectorSet(0, 0, 0, 0));
                    res = m_pGltfLoader->Load("..\\..\\cauldron-media\\SciFiHelmet\\glTF\\", "SciFiHelmet.gltf"); break;
                case 3: 
                    m_state.iblFactor = 2.0f;
                    m_roll = 0.0f; m_pitch = 0.0f; m_distance = 3.5f;
                    m_state.camera.LookAt(m_roll, m_pitch, m_distance, XMVectorSet(0, 0, 0, 0));
                    res = m_pGltfLoader->Load("..\\..\\cauldron-media\\DamagedHelmet\\glTF\\", "DamagedHelmet.gltf"); break;
                case 4: 
                    m_state.iblFactor = 0.362f;
                    m_pitch = 0.182035938f; m_roll = 1.92130506f; m_distance = 4.83333349f;
                    m_state.camera.LookAt(m_roll, m_pitch, m_distance, XMVectorSet(0.703276634f, 1.02280307f, 0.218072295f, 0));
                    res = m_pGltfLoader->Load("..\\..\\cauldron-media\\sponza\\gltf\\", "sponza.gltf"); break;
                case 5: 
                    m_state.iblFactor = 1.0f;
                    m_roll = 0.0f; m_pitch = 0.0f; m_distance = 16.0f;
                    m_state.camera.LookAt(m_roll, m_pitch, m_distance, XMVectorSet(0, 0, 0, 0));
                    res = m_pGltfLoader->Load("..\\..\\cauldron-media\\MetalRoughSpheres\\glTF\\", "MetalRoughSpheres.gltf"); break;
                }

                if (res == false)
                {
                    ImGui::OpenPopup("Error");

                    delete m_pGltfLoader;
                    m_pGltfLoader = NULL;
                }
                else
                {
                    loadingStage = m_pNode->LoadScene(m_pGltfLoader, 0);
                }

                ImGui::End();
                ImGui::EndFrame();
                return;
            }
        }

        std::vector<ResolutionInfo> supportedResolutions = {};
        CAS_Filter::GetSupportedResolutions(m_Width, m_Height, supportedResolutions);
        auto itemsGetter = [](void* data, int idx, const char** outText)
        {
            ResolutionInfo* resolutions = reinterpret_cast<ResolutionInfo*>(data);
            *outText = resolutions[idx].pName;
            return true;
        };

        int oldDisplayMode = m_currDisplayMode;
        ImGui::Combo("Render Dim", &m_currDisplayMode, itemsGetter, reinterpret_cast<void*>(&supportedResolutions.front()), static_cast<int>(supportedResolutions.size()));

        if (m_device.IsFp16Supported())
        {
            ImGui::Checkbox("Enable Packed Math", &m_state.usePackedMath);
        }

        int oldCasState = (int)m_state.CASState;
        const char* casItemNames[] =
        {
            "No Cas",
            "Cas Upsample",
            "Cas Sharpen Only",
        };
        ImGui::Combo("Cas Options", (int*)&m_state.CASState, casItemNames, _countof(casItemNames));

        ImGuiIO& io = ImGui::GetIO();

        if (io.KeysDownDuration['Q'] == 0.0f)
        {
            m_state.CASState = CAS_State_NoCas;
        }
        else if (io.KeysDownDuration['W'] == 0.0f)
        {
            m_state.CASState = CAS_State_Upsample;
        }
        else if (io.KeysDownDuration['E'] == 0.0f)
        {
            m_state.CASState = CAS_State_SharpenOnly;
        }

        if (oldDisplayMode != m_currDisplayMode || oldCasState != m_state.CASState)
        {
            m_state.renderWidth = supportedResolutions[m_currDisplayMode].Width;
            m_state.renderHeight = supportedResolutions[m_currDisplayMode].Height;

            m_device.GPUFlush();
            m_pNode->OnDestroyWindowSizeDependentResources();
            m_pNode->OnCreateWindowSizeDependentResources(&m_swapChain, &m_state, m_Width, m_Height);
        }

        float newSharpenControl = m_state.sharpenControl;
        ImGui::SliderFloat("Cas Sharpen", &newSharpenControl, 0, 1);

        if (newSharpenControl != m_state.sharpenControl)
        {
            m_state.sharpenControl = newSharpenControl;
            m_pNode->UpdateCASSharpness(m_state.sharpenControl, m_state.CASState);
        }

        const char * cameraControl[] = { "WASD", "Orbit" };
        static int cameraControlSelected = 1;
        ImGui::Combo("Camera", &cameraControlSelected, cameraControl, _countof(cameraControl));

        float CASTime = 0.0f;
        if (ImGui::CollapsingHeader("Profiler", ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::vector<TimeStamp> timeStamps = m_pNode->GetTimingValues();
            if (timeStamps.size() > 0)
            {
                for (uint32_t i = 1; i < timeStamps.size(); i++)
                {
                    float DeltaTime = ((float)(timeStamps[i].m_microseconds - timeStamps[i - 1].m_microseconds));
                    if (strcmp("CAS", timeStamps[i].m_label.c_str()) == 0)
                    {
                        CASTime = DeltaTime;
                    }
                    ImGui::Text("%-17s: %7.1f us", timeStamps[i].m_label.c_str(), DeltaTime);
                }

                //scrolling data and average computing
                static float values[128];
                values[127] = (float)(timeStamps.back().m_microseconds - timeStamps.front().m_microseconds);
                float average = values[0];
                for (uint32_t i = 0; i < 128 - 1; i++) { values[i] = values[i + 1]; average += values[i]; }
                average /= 128;

                ImGui::Text("%-17s: %7.1f us", "TotalGPUTime", average);
                ImGui::PlotLines("", values, 128, 0, "", 0.0f, 30000.0f, ImVec2(0, 80));
            }
        }
        
        if (m_state.profiling)
        {
            m_CASTimings[m_CASTimingsCurrId++] = CASTime;
            if (m_CASTimingsCurrId == _countof(m_CASTimings))
            {
                m_state.profiling = false;
                m_CASTimingsCurrId = 0;
                for (int i = 0; i < _countof(m_CASTimings); ++i)
                {
                    m_CASAvgTiming += m_CASTimings[i];
                }

                m_CASAvgTiming /= static_cast<float>(_countof(m_CASTimings));
            }
        }

        bool isHit = ImGui::Button("Start Timing", { 150, 30 });
        if (isHit)
        {
            m_state.profiling = true;
        }
        ImGui::Text("Avg Cas Time: %f", m_CASAvgTiming);

        ImGui::End();

        // If the mouse was not used by the GUI then it's for the camera
        //
        if (io.WantCaptureMouse == false) 
        {
            if ((io.KeyCtrl == false) && (io.MouseDown[0] == true))
            {
                m_roll -= io.MouseDelta.x / 100.f;
                m_pitch += io.MouseDelta.y / 100.f;
            }

            // Choose camera movement depending on setting
            //

            if (cameraControlSelected == 0)
            {
                //  WASD
                //
                m_state.camera.UpdateCameraWASD(m_roll, m_pitch, io.KeysDown, io.DeltaTime);
            }
            else if (cameraControlSelected == 1)
            {
                //  Orbiting
                //
                m_distance -= static_cast<float>(io.MouseWheel) / 3.0f;
                m_distance = std::max<float>(m_distance, 0.1f);

                bool panning = (io.KeyCtrl == true) && (io.MouseDown[0] == true);

                m_state.camera.UpdateCameraPolar(m_roll, m_pitch, panning ? -io.MouseDelta.x / 100.0f : 0.0f, panning ? io.MouseDelta.y / 100.0f : 0.0f, m_distance );
            }
        }
    }
    
    // Set animation time
    //
    if (m_bPlay)
    {
        m_time += static_cast<float>(m_deltaTime) / 1000.0f;
    }    

    // Animate and transform the scene
    //
    if (m_pGltfLoader)
    {
        m_pGltfLoader->SetAnimationTime(0, m_time);
        m_pGltfLoader->TransformScene(0, XMMatrixIdentity());
    }

    m_state.time = m_time;

    // Do Render frame using AFR 
    //
    m_pNode->OnRender(&m_state, &m_swapChain);
    
    m_swapChain.Present();
}


//--------------------------------------------------------------------------------------
//
// WinMain
//
//--------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    LPCSTR Name = "CAS DX12 Sample v1.0";
    uint32_t Width = 1280;
    uint32_t Height = 720;
    
    // create new DX sample
    return RunFramework(hInstance, lpCmdLine, nCmdShow, Width, Height, new CAS_Sample(Name));
}

