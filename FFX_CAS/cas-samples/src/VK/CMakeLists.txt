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

project (CAS_Sample_VK)

set(sources 
    CAS_CS.cpp
    CAS_CS.h
    CAS_Sample.cpp
    CAS_Sample.h
    CAS_Renderer.cpp
    CAS_Renderer.h
    stdafx.cpp
    stdafx.h)
    
source_group("Sources"            FILES ${sources})   

set(Shaders_src
    ${CMAKE_CURRENT_SOURCE_DIR}/CAS_Shader.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/CAS_RenderPS.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ffx-cas-headers/ffx_a.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ffx-cas-headers/ffx_cas.h)

source_group("Shaders"         FILES ${Shaders_src})

foreach(fullFileName ${Shaders_src})
    get_filename_component(file ${fullFileName} NAME)
    message("Generating custom command for ${fullFileName}")
    add_custom_command(
        OUTPUT   ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibVK/${file}
        PRE_BUILD
        COMMAND cmake -E make_directory ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibVK
        COMMAND cmake -E copy ${fullFileName}  ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibVK
        MAIN_DEPENDENCY  ${fullFileName}
        COMMENT "Updating ${file}, copying shader to bin" 
    )
endforeach()

add_executable(${PROJECT_NAME} WIN32 ${sources} ${Shaders_src}) 
target_link_libraries (${PROJECT_NAME} LINK_PUBLIC Framework_VK ImGUI Vulkan::Vulkan)
target_include_directories (${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../ffx-cas-headers)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin")


IF (MSVC)
    IF (CMAKE_MAJOR_VERSION LESS 3)
        MESSAGE(WARNING "CMake version 3.0 or newer is required use build variable TARGET_FILE")
    ELSE()
        ADD_CUSTOM_COMMAND(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND "mt.exe" -manifest \"${CMAKE_CURRENT_SOURCE_DIR}\\dpiawarescaling.manifest\" -inputresource:\"$<TARGET_FILE:${PROJECT_NAME}>\"\;\#1 -outputresource:\"$<TARGET_FILE:${PROJECT_NAME}>\"\;\#1
            COMMENT "Adding display aware manifest..." 
        )
    ENDIF()
ENDIF(MSVC)