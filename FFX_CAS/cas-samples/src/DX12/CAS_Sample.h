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

#pragma once

#include "CAS_Renderer.h"

//
// This is the main class, it manages the state of the sample and does all the high level work without touching the GPU directly.
// This class uses the GPU via the the SampleRenderer class. We would have a SampleRenderer instance for each GPU.
//
// This class takes care of:
//
//    - loading a scene (just the CPU data)
//    - updating the camera
//    - keeping track of time
//    - handling the keyboard
//    - updating the animation
//    - building the UI (but do not renders it)
//    - uses the SampleRenderer to update all the state to the GPU and do the rendering
//

class CAS_Sample : public FrameworkWindows
{
public:
    CAS_Sample(LPCSTR name);
    void OnCreate(HWND hWnd);
    void OnDestroy();
    void OnRender();
    bool OnEvent(MSG msg);
    void OnResize(uint32_t Width, uint32_t Height);
    void SetFullScreen(bool fullscreen);
    
private:
    Device                m_device;
    SwapChain             m_swapChain;

    GLTFCommon           *m_pGltfLoader;

    CAS_Renderer         *m_pNode;
    CAS_Renderer::State   m_state;

    float                 m_distance;
    float                 m_roll;
    float                 m_pitch;

    float                 m_time;             // WallClock in seconds.
    double                m_deltaTime;        // The elapsed time in milliseconds since the previous frame.
    double                m_lastFrameTime;

    bool                  m_bPlay;
    
    int                   m_currDisplayMode;

    // Profiling info for CAS
    int                   m_CASTimingsCurrId;
    float                 m_CASTimings[300];
    float                 m_CASAvgTiming;
};
