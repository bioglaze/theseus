@echo off
if not exist ..\build\shaders mkdir ..\build\shaders
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E unlitMS -all-resources-bound -T ms_6_5 shaders/hlsl/unlit.hlsl -Fo ../build/shaders/unlit_ms.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E unlitVS -all-resources-bound -T vs_6_5 shaders/hlsl/unlit.hlsl -Fo ../build/shaders/unlit_vs.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E unlitPS -all-resources-bound -T ps_6_5 shaders/hlsl/unlit.hlsl -Fo ../build/shaders/unlit_ps.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E uiVS -all-resources-bound -T vs_6_5 shaders/hlsl/ui.hlsl -Fo ../build/shaders/ui_vs.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E uiPS -all-resources-bound -T ps_6_5 shaders/hlsl/ui.hlsl -Fo ../build/shaders/ui_ps.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E fullscreenVS -all-resources-bound -T vs_6_5 shaders/hlsl/fullscreen.hlsl -Fo ../build/shaders/fullscreen_vs.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E fullscreenPS -all-resources-bound -T ps_6_5 shaders/hlsl/fullscreen.hlsl -Fo ../build/shaders/fullscreen_ps.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E fullscreenAdditivePS -all-resources-bound -T ps_6_5 shaders/hlsl/fullscreen.hlsl -Fo ../build/shaders/fullscreen_additive_ps.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E skyboxVS -all-resources-bound -T vs_6_5 shaders/hlsl/skybox.hlsl -Fo ../build/shaders/skybox_vs.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E skyboxPS -all-resources-bound -T ps_6_5 shaders/hlsl/skybox.hlsl -Fo ../build/shaders/skybox_ps.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E standardVS -all-resources-bound -T vs_6_5 shaders/hlsl/standard.hlsl -Fo ../build/shaders/standard_vs.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E standardPS -all-resources-bound -T ps_6_5 shaders/hlsl/standard.hlsl -Fo ../build/shaders/standard_ps.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E bloomThreshold -all-resources-bound -T cs_6_5 shaders/hlsl/bloom.hlsl -Fo ../build/shaders/bloom_threshold.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E bloomBlur -all-resources-bound -T cs_6_5 shaders/hlsl/bloom.hlsl -Fo ../build/shaders/bloom_blur.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E bloomDownsample -all-resources-bound -T cs_6_5 shaders/hlsl/bloom.hlsl -Fo ../build/shaders/downsample.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E bloomCombine -all-resources-bound -T cs_6_5 shaders/hlsl/bloom.hlsl -Fo ../build/shaders/bloom_combine.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E momentsVS -all-resources-bound -T vs_6_5 shaders/hlsl/moments.hlsl -Fo ../build/shaders/moments_vs.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E momentsPS -all-resources-bound -T ps_6_5 shaders/hlsl/moments.hlsl -Fo ../build/shaders/moments_ps.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E depthNormalsVS -all-resources-bound -T vs_6_5 shaders/hlsl/depthnormals.hlsl -Fo ../build/shaders/depthnormals_vs.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E depthNormalsPS -all-resources-bound -T ps_6_5 shaders/hlsl/depthnormals.hlsl -Fo ../build/shaders/depthnormals_ps.spv
%VULKAN_SDK%/bin/dxc -Ges -spirv -fspv-target-env=vulkan1.2 -E cullLights -all-resources-bound -T cs_6_5 shaders/hlsl/lightculler.hlsl -Fo ../build/shaders/lightculler.spv
pause

