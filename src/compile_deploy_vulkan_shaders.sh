mkdir -p ../build/shaders
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E fullscreenVS -all-resources-bound -T vs_6_5 shaders/hlsl/fullscreen.hlsl -Fo ../build/shaders/fullscreen_vs.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E fullscreenPS -all-resources-bound -T ps_6_5 shaders/hlsl/fullscreen.hlsl -Fo ../build/shaders/fullscreen_ps.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E fullscreenAdditivePS -all-resources-bound -T ps_6_5 shaders/hlsl/fullscreen.hlsl -Fo ../build/shaders/fullscreen_additive_ps.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E unlitVS -all-resources-bound -T vs_6_5 shaders/hlsl/unlit.hlsl -Fo ../build/shaders/unlit_vs.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E unlitPS -all-resources-bound -T ps_6_5 shaders/hlsl/unlit.hlsl -Fo ../build/shaders/unlit_ps.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E uiVS -all-resources-bound -T vs_6_5 shaders/hlsl/ui.hlsl -Fo ../build/shaders/ui_vs.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E uiPS -all-resources-bound -T ps_6_5 shaders/hlsl/ui.hlsl -Fo ../build/shaders/ui_ps.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E skyboxVS -all-resources-bound -T vs_6_5 shaders/hlsl/skybox.hlsl -Fo ../build/shaders/skybox_vs.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E skyboxPS -all-resources-bound -T ps_6_5 shaders/hlsl/skybox.hlsl -Fo ../build/shaders/skybox_ps.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E standardVS -all-resources-bound -T vs_6_5 shaders/hlsl/standard.hlsl -Fo ../build/shaders/standard_vs.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E standardPS -all-resources-bound -T ps_6_5 shaders/hlsl/standard.hlsl -Fo ../build/shaders/standard_ps.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E bloomThreshold -all-resources-bound -T cs_6_5 shaders/hlsl/bloom.hlsl -Fo ../build/shaders/bloom_threshold.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E bloomBlur -all-resources-bound -T cs_6_5 shaders/hlsl/bloom.hlsl -Fo ../build/shaders/bloom_blur.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E bloomDownsample -all-resources-bound -T cs_6_5 shaders/hlsl/bloom.hlsl -Fo ../build/shaders/downsample.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E momentsVS -all-resources-bound -T vs_6_5 shaders/hlsl/moments.hlsl -Fo ../build/shaders/moments_vs.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E momentsPS -all-resources-bound -T ps_6_5 shaders/hlsl/moments.hlsl -Fo ../build/shaders/moments_ps.spv

