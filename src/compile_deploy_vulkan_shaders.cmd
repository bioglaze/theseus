@echo off
if not exist ..\build\shaders mkdir ..\build\shaders
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E unlitVS -all-resources-bound -T vs_6_5 shaders/hlsl/unlit.hlsl -Fo ../build/shaders/unlit_vs.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E unlitFS -all-resources-bound -T ps_6_5 shaders/hlsl/unlit.hlsl -Fo ../build/shaders/unlit_fs.spv
pause

