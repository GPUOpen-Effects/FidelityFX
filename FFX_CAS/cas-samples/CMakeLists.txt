# CAS Sample
# 
# Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

cmake_minimum_required(VERSION 3.4)
set(CMAKE_GENERATOR_PLATFORM x64)

project (CAS_Sample_${GFX_API})

# ouput exe to bin directory
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/bin)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_HOME_DIRECTORY}/bin )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

if(GFX_API STREQUAL DX12)        
    add_subdirectory(src/DX12)   
elseif(GFX_API STREQUAL VK)
    find_package(Vulkan REQUIRED)
    add_subdirectory(src/VK)
else()
    message(STATUS "----------------------------------------------------------------------------------------")
    message(STATUS "")
    message(STATUS "** Almost there!!")
    message(STATUS "")
    message(STATUS " This framework supports DX12 or VULKAN, you need to invoke cmake in one of these ways:")
    message(STATUS "")
    message(STATUS " Examples:")
    message(STATUS "    cmake <project_root_dir> -DGFX_API=DX12")
    message(STATUS "    cmake <project_root_dir> -DGFX_API=VK")
    message(STATUS "")
    message(STATUS "----------------------------------------------------------------------------------------")
    message(FATAL_ERROR "")
endif()

# reference libs used by both backends
add_subdirectory(libs/cauldron)