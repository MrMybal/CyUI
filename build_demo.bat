@echo off
rem Builds the CyUI demo with MSVC, without CMake. Run from any prompt.
setlocal
set ROOT=%~dp0
if not defined VCINSTALLDIR (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
)
if not exist "%ROOT%build" mkdir "%ROOT%build"
pushd "%ROOT%build"
set IMGUI=%ROOT%third_party\imgui
cl /nologo /std:c++20 /O2 /EHsc /W4 /utf-8 /MP /DUNICODE /D_UNICODE /DCYUI_HAS_AUDIO=1 /DCYUI_HAS_GALLERY=1 /DCYUI_HAS_CANVAS=1 /DCYUI_HAS_PLOTS=1 /DCYUI_HAS_CURVES=1 /DCYUI_HAS_NODES=1 /DCYUI_HAS_TIMELINE=1 /DCYUI_HAS_AUDIOVIEWS=1 /DCYUI_HAS_CODE=1 /DCYUI_HAS_DIFF=1 /DCYUI_HAS_CODEEDITOR=1 /DCYUI_HAS_INSPECTOR=1 /DCYUI_HAS_ASSETS=1 /DCYUI_HAS_IMAGES=1 /DCYUI_HAS_VIEWPORT=1 /DCYUI_HAS_COMMANDS=1 /DCYUI_HAS_DIAGNOSTICS=1 /DCYUI_HAS_DOCUMENTVIEWS=1 /DCYUI_HAS_WEB=1 ^
   /I"%ROOT%third_party\texteditor" /I"%ROOT%include" /I"%IMGUI%" /I"%IMGUI%\backends" ^
   "%ROOT%src\Theme.cpp" "%ROOT%src\Widgets.cpp" "%ROOT%src\Layout.cpp" "%ROOT%src\Gallery.cpp" ^
   "%ROOT%src\modules\*.cpp" "%ROOT%third_party\texteditor\TextEditor.cpp" ^
   "%ROOT%examples\demo_win32_dx11\main.cpp" ^
   "%IMGUI%\imgui.cpp" "%IMGUI%\imgui_draw.cpp" "%IMGUI%\imgui_tables.cpp" "%IMGUI%\imgui_widgets.cpp" "%IMGUI%\imgui_demo.cpp" ^
   "%IMGUI%\backends\imgui_impl_win32.cpp" "%IMGUI%\backends\imgui_impl_dx11.cpp" ^
   /Fe:cyui_demo.exe /link /SUBSYSTEM:WINDOWS d3d11.lib dxgi.lib d3dcompiler.lib dwmapi.lib user32.lib gdi32.lib shell32.lib
set ERR=%ERRORLEVEL%
popd
exit /b %ERR%
