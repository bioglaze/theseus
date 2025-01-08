mkdir -p ../build/shaders
glslc -fshader-stage=vertex -fentry-point=fullscreenVS shaders/hlsl/fullscreen.hlsl -o ../build/shaders/fullscreen_vs.spv
glslc -fshader-stage=fragment -fentry-point=fullscreenPS shaders/hlsl/fullscreen.hlsl -o ../build/shaders/fullscreen_ps.spv
glslc -fshader-stage=fragment -fentry-point=fullscreenAdditivePS shaders/hlsl/fullscreen.hlsl -o ../build/shaders/fullscreen_additive_ps.spv
glslc -fshader-stage=vertex -fentry-point=unlitVS shaders/hlsl/unlit.hlsl -o ../build/shaders/unlit_vs.spv
glslc -fshader-stage=fragment -fentry-point=unlitPS shaders/hlsl/unlit.hlsl -o ../build/shaders/unlit_ps.spv
glslc -fshader-stage=vertex -fentry-point=uiVS shaders/hlsl/ui.hlsl -o ../build/shaders/ui_vs.spv
glslc -fshader-stage=fragment -fentry-point=uiPS shaders/hlsl/ui.hlsl -o ../build/shaders/ui_ps.spv
glslc -fshader-stage=vertex -fentry-point=skyboxVS shaders/hlsl/skybox.hlsl -o ../build/shaders/skybox_vs.spv
glslc -fshader-stage=fragment -fentry-point=skyboxPS shaders/hlsl/skybox.hlsl -o ../build/shaders/skybox_ps.spv
glslc -fshader-stage=vertex -fentry-point=standardVS shaders/hlsl/standard.hlsl -o ../build/shaders/standard_vs.spv
glslc -fshader-stage=fragment -fentry-point=standardPS shaders/hlsl/standard.hlsl -o ../build/shaders/standard_ps.spv
glslc -fshader-stage=compute -fentry-point=bloomThreshold shaders/hlsl/bloom.hlsl -o ../build/shaders/bloom_threshold.spv
glslc -fshader-stage=compute -fentry-point=bloomBlur shaders/hlsl/bloom.hlsl -o ../build/shaders/bloom_blur.spv
glslc -fshader-stage=compute -fentry-point=bloomDownsample shaders/hlsl/bloom.hlsl -o ../build/shaders/downsample.spv
glslc -fshader-stage=compute -fentry-point=bloomCombine shaders/hlsl/bloom.hlsl -o ../build/shaders/bloom_combine.spv
glslc -fshader-stage=vertex -fentry-point=momentsVS shaders/hlsl/moments.hlsl -o ../build/shaders/moments_vs.spv
glslc -fshader-stage=fragment -fentry-point=momentsPS shaders/hlsl/moments.hlsl -o ../build/shaders/moments_ps.spv

