# theseus
Theseus Game Engine

Author: [Timo Wirén](https://twiren.kapsi.fi)

![Screenshot](screenshot.jpg)

This will become the successor to [Aether3D engine](https://github.com/bioglaze/aether3d).
Very early days, only renders hard-coded cubes, without lights etc.

# Features

  - Modern Vulkan 1.3 and Metal 3 renderers
  - Fast compile times
  - Loads .tga and .dds textures
  - Shader hot-reloading
  
# Platforms

  - Windows (VS 2022 project files included)
  - macOS/iOS
  - Linux is coming soon

# Building

  - macOS:
    - build theseus.xcodeproj and copy the resulting framework to src/samples/hello_mac_ios
    - build and run hello_mac_ios/hello.xcodeproj
