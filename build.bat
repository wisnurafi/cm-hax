@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
set "OUT_DIR=%ROOT%build"
set "OBJ_DIR=%OUT_DIR%\obj"
set "SDK_NAME=CombatMaster_SDK_Full.hpp"
set "TARGET=%~1"
if not defined TARGET set "TARGET=all"

if /I "%TARGET%"=="help" goto :usage
if /I "%TARGET%"=="--help" goto :usage
if /I "%TARGET%"=="/?" goto :usage

echo ============================================================
echo   Combat Master - Build
echo ============================================================
echo.

if /I "%TARGET%"=="clean" goto :clean

call :find_vs
if errorlevel 1 exit /b 1

echo [*] Using Visual Studio environment:
echo     %FOUND_VS%
call "%FOUND_VS%" >nul
if errorlevel 1 (
    echo [!] Failed to load the Visual Studio build environment.
    exit /b 1
)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

set "COMMON_FLAGS=/nologo /EHsc /std:c++17 /O2 /W3"

if /I "%TARGET%"=="all" (
    call :build_dumper
    if errorlevel 1 exit /b 1
    call :build_menu
    if errorlevel 1 exit /b 1
    call :build_injector
    if errorlevel 1 exit /b 1
    goto :done
)

if /I "%TARGET%"=="dumper" (
    call :build_dumper
    if errorlevel 1 exit /b 1
    goto :done
)

if /I "%TARGET%"=="menu" (
    call :build_menu
    if errorlevel 1 exit /b 1
    goto :done
)

if /I "%TARGET%"=="injector" (
    call :build_injector
    if errorlevel 1 exit /b 1
    goto :done
)

echo [!] Unknown build target: %TARGET%
goto :usage_error

:find_vs
set "FOUND_VS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" (
            set "FOUND_VS=%%I\VC\Auxiliary\Build\vcvars64.bat"
        )
    )
)

if defined FOUND_VS exit /b 0

for %%V in (2022 2019 2017) do (
    for %%E in (Enterprise Professional Community BuildTools) do (
        if exist "C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" (
            set "FOUND_VS=C:\Program Files\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat"
            exit /b 0
        )
        if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" (
            set "FOUND_VS=C:\Program Files (x86)\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat"
            exit /b 0
        )
    )
)

echo [!] Visual Studio C++ build tools were not found.
echo [!] Install Visual Studio 2022 or Build Tools with "Desktop development with C++".
exit /b 1

:prepare_obj
set "THIS_OBJ_DIR=%OBJ_DIR%\%~1"
if not exist "%THIS_OBJ_DIR%" mkdir "%THIS_OBJ_DIR%"
exit /b 0

:build_dumper
call :prepare_obj dumper
echo.
echo [*] Building dumper.dll...
cl %COMMON_FLAGS% /LD "%ROOT%dumper.cpp" /Fo"%THIS_OBJ_DIR%\\" /Fe"%OUT_DIR%\dumper.dll" /link /INCREMENTAL:NO
if errorlevel 1 (
    echo [!] Failed to build dumper.dll
    exit /b 1
)
echo [+] Built %OUT_DIR%\dumper.dll
exit /b 0

:build_injector
call :prepare_obj injector
echo.
echo [*] Building injector.exe...
cl %COMMON_FLAGS% "%ROOT%injector.cpp" /Fo"%THIS_OBJ_DIR%\\" /Fe"%OUT_DIR%\injector.exe" /link /INCREMENTAL:NO advapi32.lib
if errorlevel 1 (
    echo [!] Failed to build injector.exe
    exit /b 1
)
echo [+] Built %OUT_DIR%\injector.exe
exit /b 0

:resolve_minhook
set "MINHOOK_INCLUDE="
set "MINHOOK_INPUTS="

if exist "%ROOT%minhook\include\MinHook.h" (
    set "MINHOOK_INCLUDE=%ROOT%minhook\include"
    if exist "%ROOT%minhook\src\hook.c" if exist "%ROOT%minhook\src\buffer.c" if exist "%ROOT%minhook\src\trampoline.c" if exist "%ROOT%minhook\src\hde\hde64.c" (
        set "MINHOOK_INPUTS=%ROOT%minhook\src\hook.c %ROOT%minhook\src\buffer.c %ROOT%minhook\src\trampoline.c %ROOT%minhook\src\hde\hde64.c"
        exit /b 0
    )

    for %%L in ("%ROOT%minhook\lib\libMinHook.x64.lib" "%ROOT%minhook\lib\minhook.lib" "%ROOT%minhook\build\VC17\libMinHook.x64.lib" "%ROOT%minhook\build\VC16\libMinHook.x64.lib" "%ROOT%minhook\build\libMinHook.x64.lib") do (
        if not defined MINHOOK_INPUTS if exist "%%~fL" set "MINHOOK_INPUTS=%%~fL"
    )
)

if not defined MINHOOK_INPUTS if defined VCPKG_ROOT (
    if exist "%VCPKG_ROOT%\installed\x64-windows\include\MinHook.h" (
        set "MINHOOK_INCLUDE=%VCPKG_ROOT%\installed\x64-windows\include"
        for %%L in ("%VCPKG_ROOT%\installed\x64-windows\lib\*minhook*.lib" "%VCPKG_ROOT%\installed\x64-windows\lib\*MinHook*.lib") do (
            if not defined MINHOOK_INPUTS if exist "%%~fL" set "MINHOOK_INPUTS=%%~fL"
        )
    )
)

if defined MINHOOK_INCLUDE if defined MINHOOK_INPUTS exit /b 0

echo [!] MinHook was not found, so esp_imgui.dll cannot be built.
echo [!] Put MinHook at "%ROOT%minhook" with include\MinHook.h and either src files or a x64 .lib.
echo [!] You can still build the other targets with:
echo     build.bat dumper
echo     build.bat injector
exit /b 1

:build_menu
call :resolve_minhook
if errorlevel 1 exit /b 1
call :prepare_obj menu
echo.
echo [*] Building esp_imgui.dll...
cl %COMMON_FLAGS% /LD ^
    /I"%ROOT%imgui" ^
    /I"%ROOT%imgui\backends" ^
    /I"%MINHOOK_INCLUDE%" ^
    /I"%OUT_DIR%" ^
    "%ROOT%esp.cpp" ^
    "%ROOT%imgui\imgui.cpp" ^
    "%ROOT%imgui\imgui_draw.cpp" ^
    "%ROOT%imgui\imgui_tables.cpp" ^
    "%ROOT%imgui\imgui_widgets.cpp" ^
    "%ROOT%imgui\backends\imgui_impl_win32.cpp" ^
    "%ROOT%imgui\backends\imgui_impl_dx11.cpp" ^
    %MINHOOK_INPUTS% ^
    /Fo"%THIS_OBJ_DIR%\\" ^
    /Fe"%OUT_DIR%\esp_imgui.dll" ^
    /link /INCREMENTAL:NO d3d11.lib dxgi.lib user32.lib gdi32.lib
if errorlevel 1 (
    echo [!] Failed to build esp_imgui.dll
    exit /b 1
)
echo [+] Built %OUT_DIR%\esp_imgui.dll
exit /b 0

:copy_sdk
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if exist "%ROOT%%SDK_NAME%" (
    copy /y "%ROOT%%SDK_NAME%" "%OUT_DIR%\%SDK_NAME%" >nul
    echo [+] Included %OUT_DIR%\%SDK_NAME%
    exit /b 0
)
if exist "%OUT_DIR%\%SDK_NAME%" (
    echo [+] Included %OUT_DIR%\%SDK_NAME%
)
exit /b 0

:clean
set "SDK_BACKUP="
if exist "%OUT_DIR%\%SDK_NAME%" (
    set "SDK_BACKUP=%TEMP%\cm_sdk_%RANDOM%_%SDK_NAME%"
    copy /y "%OUT_DIR%\%SDK_NAME%" "%SDK_BACKUP%" >nul
)
if exist "%OUT_DIR%" (
    echo [*] Removing "%OUT_DIR%"...
    rmdir /s /q "%OUT_DIR%"
)
if exist "%OUT_DIR%" (
    echo [!] Clean left one or more locked files in "%OUT_DIR%".
    echo [!] Close any process using the DLLs, then run build.bat clean again.
)
if defined SDK_BACKUP if exist "%SDK_BACKUP%" (
    mkdir "%OUT_DIR%" >nul 2>nul
    copy /y "%SDK_BACKUP%" "%OUT_DIR%\%SDK_NAME%" >nul
    del /q "%SDK_BACKUP%" >nul 2>nul
    echo [+] Preserved %OUT_DIR%\%SDK_NAME%
)
call :copy_sdk
echo [+] Clean complete.
exit /b 0

:done
call :copy_sdk
echo.
echo ============================================================
echo   BUILD COMPLETE
echo ============================================================
echo   Output folder:
echo     %OUT_DIR%
echo.
echo   Run:
echo     %OUT_DIR%\injector.exe
echo.
exit /b 0

:usage_error
exit /b 1

:usage
echo Usage:
echo   build.bat [all^|injector^|dumper^|menu^|clean]
echo.
echo Default target is "all".
exit /b 0
