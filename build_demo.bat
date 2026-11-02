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
cl /nologo /std:c++17 /O2 /EHsc /W4 /utf-8 /MP /DUNICODE /D_UNICODE ^
   /I"%ROOT%include" /I"%IMGUI%" /I"%IMGUI%\backends" ^
   "%ROOT%src\Theme.cpp" "%ROOT%src\Widgets.cpp" "%ROOT%src\Layout.cpp" "%ROOT%src\Gallery.cpp" ^
   "%ROOT%examples\demo_win32_dx11\main.cpp" ^
   "%IMGUI%\imgui.cpp" "%IMGUI%\imgui_draw.cpp" "%IMGUI%\imgui_tables.cpp" "%IMGUI%\imgui_widgets.cpp" "%IMGUI%\imgui_demo.cpp" ^
   "%IMGUI%\backends\imgui_impl_win32.cpp" "%IMGUI%\backends\imgui_impl_dx11.cpp" ^
   /Fe:cyui_demo.exe /link /SUBSYSTEM:WINDOWS d3d11.lib dxgi.lib d3dcompiler.lib dwmapi.lib user32.lib gdi32.lib shell32.lib
set ERR=%ERRORLEVEL%
popd
exit /b %ERR%
