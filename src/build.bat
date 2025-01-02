@if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
  @if "%COMPILER_FOUND%"  == "" (
      call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
      SET COMPILER_FOUND=1
  )
) else (
  if "%COMPILER_FOUND%"  == "" (
      call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
      SET COMPILER_FOUND=1
  )
)

@if %PROCESSOR_ARCHITECTURE% == AMD64 (SET SIMD=/DSIMD_SSE3)
@if %PROCESSOR_ARCHITECTURE% == ARM64 (SET SIMD=/DSIMD_NEON)

cl -c unity.cpp -W4 /DVK_USE_PLATFORM_WIN32_KHR /D_CRT_SECURE_NO_WARNINGS %SIMD% /nologo /Iinclude /Ivideo /Icore /I%VULKAN_SDK%\Include /std:c++17 -Zi /FC /diagnostics:column
cl samples\hello\hello.cpp thirdparty\imgui\*.cpp unity.obj -Zi -W4 /nologo /DVK_USE_PLATFORM_WIN32_KHR /D_CRT_SECURE_NO_WARNINGS %SIMD% /Iinclude /Ivideo /Icore /Ithirdparty/imgui/ /std:c++17 /FC /diagnostics:column /link /INCREMENTAL:NO /LIBPATH:%VULKAN_SDK%\Lib vulkan-1.lib /OUT:..\build\hello.exe
cl tools\editor\*.cpp thirdparty\imgui\*.cpp unity.obj -W4 /nologo /DVK_USE_PLATFORM_WIN32_KHR /D_CRT_SECURE_NO_WARNINGS %SIMD% /Iinclude /Ivideo /Icore /Ithirdparty/imgui/ /std:c++17 /FC /diagnostics:column /link /INCREMENTAL:NO /LIBPATH:%VULKAN_SDK%\Lib Comdlg32.lib vulkan-1.lib /OUT:..\build\editor.exe
mt -manifest tools\editor\editor.manifest -outputresource:..\build\editor.exe;1
cl tools\convert_obj\convert_obj.cpp thirdparty\meshoptimizer\*.cpp unity.obj -Zi -W4 /nologo /DVK_USE_PLATFORM_WIN32_KHR /D_CRT_SECURE_NO_WARNINGS %SIMD% /Iinclude /Ivideo /Icore /Ithirdparty/meshoptimizer /std:c++17 /FC /diagnostics:column /link /INCREMENTAL:NO /LIBPATH:%VULKAN_SDK%\Lib user32.lib vulkan-1.lib /OUT:..\build\convert_obj.exe

