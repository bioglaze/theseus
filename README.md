# theseus
Theseus Game Engine

Author: [Timo Wirén](https://twiren.kapsi.fi)

![Screenshot](screenshot.jpg)

This will become the successor to [Aether3D engine](https://github.com/bioglaze/aether3d).

# Features

  - Modern Vulkan 1.3 and Metal 3 renderers
  - Fast compile times
  - Loads .tga and .dds textures
  - OBJ mesh converter
  - Shader hot-reloading
  
# Platforms

  - Windows (VS 2022 project files included)
  - macOS (Apple Silicon)/iOS
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
    
  - macOS/Xcode:
    - build theseus.xcodeproj and copy the resulting framework to src/samples/hello_mac_ios
    - build and run hello_mac_ios/hello.xcodeproj
    - OBJ mesh converter can be build by running "make toolz" in src.
    
  - Linux:
    - First build ImGUI: `make imgui`. You only need to do this once, unless you want to modify ImGUI later.
    - Then build the engine: `make engine`. Build artifacts are copied to theseus/build
    - Shaders can be compiled by running compile_deploy_vulkan_shaders.sh
    - OBJ mesh converter can be built by running "make toolz" in src.
