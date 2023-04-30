mkdir -p ../build/shaders
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E fullscreenVS -all-resources-bound -T vs_6_5 shaders/hlsl/fullscreen.hlsl -Fo ../build/shaders/fullscreen_vs.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E fullscreenPS -all-resources-bound -T ps_6_5 shaders/hlsl/fullscreen.hlsl -Fo ../build/shaders/fullscreen_ps.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E unlitVS -all-resources-bound -T vs_6_5 shaders/hlsl/unlit.hlsl -Fo ../build/shaders/unlit_vs.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E unlitPS -all-resources-bound -T ps_6_5 shaders/hlsl/unlit.hlsl -Fo ../build/shaders/unlit_ps.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E skyboxVS -all-resources-bound -T vs_6_5 shaders/hlsl/skybox.hlsl -Fo ../build/shaders/skybox_vs.spv
dxc -Ges -spirv -fspv-target-env=vulkan1.3 -E skyboxPS -all-resources-bound -T ps_6_5 shaders/hlsl/skybox.hlsl -Fo ../build/shaders/skybox_ps.spv