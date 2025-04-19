# Theseus Game Engine

Author: [Timo Wirén](https://twiren.kapsi.fi)

![Screenshot](screenshot.jpg)

This will become the successor to [Aether3D engine](https://github.com/bioglaze/aether3d).

# Features

  - Modern Vulkan 1.3 and Metal 3 renderers
  - Fast compile times
  - Loads .tga and .dds textures
  - Scene Editor implemented using Dear ImGui
  - OBJ mesh converter
  - Shader hot-reloading

# Platforms

  - Windows, only AMD64 tested, but ARM64 might also work.
  - macOS (Apple silicon)
  - Linux support (requires Wayland)

# Building

  - Windows/Visual Studio
    - compile shaders: src/compile_deploy_vulkan_shaders.cmd
    - build src/visualstudio/TheseusEngine.vcxproj
    - build and run src/samples/hello/visualstudio/Hello.vcxproj

  - Windows/command line:
    - compile shaders: src/compile_deploy_vulkan_shaders.cmd
    - run src/build.bat
    - run build/hello.exe
    
  - macOS/command line:
    - First build ImGui: `make imgui`. You only need to do this once, unless you want to modify/update ImGui later.
    - Then build the engine: `make engine`. Build artifacts are copied to theseus/build
    - OBJ mesh converter and Editor can be built by running `make toolz` in src.
    
  - Linux:
    - First build ImGui: `make imgui`. You only need to do this once, unless you want to modify/update ImGui later.
    - Then build the engine: `make engine`. Build artifacts are copied to theseus/build
    - Shaders can be compiled by running compile_deploy_vulkan_shaders.sh
    - OBJ mesh converter and Editor can be built by running `make toolz` in src.

# Included third-party libraries
  - meshoptimizer by Arseny Kapoulkine
  - Dear ImGUI by Omar Cornut
  - Metal-Cpp by Apple