# FidelityFX CAS (Contrast Adaptive Sharpening) v1.0

Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

## Contrast Adaptive Sharpening (CAS)

Contrast Adaptive Sharpening (CAS) is a low overhead adaptive sharpening algorithm with optional up-sampling. The technique is developed by Timothy Lottes (creator of FXAA) and was created to provide natural sharpness without artifacts. This directory contains the source code for CAS as well as samples demonstrating the technique (with source). The directory structure is as follows:

- cas-samples contains the source code for the above 2 CAS samples
- cauldron-media is a submodule which contains the art assets for the above CAS samples
- ffx-cas-headers contains the headers that implement the CAS algorithm

You can find the binaries for CAS in the release section on github. 

## Build Instructions

The CAS samples are written in C++ and require CMake and Visual Studio to be built. To build the samples, follow the below steps:

 - Run FFX_CAS\cas-samples\build\GenerateSolutions.bat which creates the FFX_CAS\cas-samples\build\DX12 and FFX_CAS\cas-samples\build\VK folder
 - Go into FFX_CAS\cas-samples\build\DX12 or FFX_CAS\cas-samples\build\VK and open the CAS_Sample_DX12/VK.sln files
 - Build the projects and run it (you should see a 3D helmet). 

## Running Instructions

When running the samples, you can use the below options to test different configurations of CAS:

 - You can make the demo full screen via alt-enter. You can set the render resolution which decides how much CAS will have to upscale the image when CAS is enabled (without enabling, it’s a naïve upscale). The GUI will also let you choose different CAS modes (sharpen only or up-sample) with packed and unpacked versions. 
 - You can use the 'q', 'w' and 'e' keys to set different CAS modes. 'q' sets it to no CAS, 'w' sets it to up-sampling, and 'e' sets it to sharpen only.
 - The code implementing CAS can be found in FFX_CAS\cas-samples\src

You can find the documentation for the CAS algorithm and how to implement it using the FFX CAS headers at ffx-cas-headers\ffx_cas.h.