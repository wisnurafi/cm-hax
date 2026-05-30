@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
set "BUILD_ROOT=%ROOT%build"
set "RELEASE_ROOT=%ROOT%release"
set "SDK_NAME=CombatMaster_SDK_Full.hpp"

rem ----------------------------------------------------------------
rem Argument parsing.
rem
rem Usage:
rem   build.bat                 -> all (Release)
rem   build.bat <target>        -> single target (Release)
rem   build.bat <target> debug  -> single target (Debug)
rem   build.bat setup           -> clone third_party deps if missing
rem   build.bat package         -> build Release + zip into release\
rem   build.bat clean           -> wipe build\
rem
rem Targets: all | injector | dumper | menu | setup | package | clean
rem ----------------------------------------------------------------

set "TARGET=%~1"
set "CONFIG_ARG=%~2"
if not defined TARGET set "TARGET=all"
if /I "%TARGET%"=="help"   goto :usage
if /I "%TARGET%"=="--help" goto :usage
if /I "%TARGET%"=="/?"     goto :usage

set "CONFIG=Release"
if /I "%CONFIG_ARG%"=="debug"   set "CONFIG=Debug"
if /I "%CONFIG_ARG%"=="release" set "CONFIG=Release"

set "OUT_DIR=%BUILD_ROOT%\%CONFIG%"
set "OBJ_ROOT=%OUT_DIR%\obj"

echo ============================================================
echo   Cm-Hax  -  %CONFIG% build
echo ============================================================
echo.

if /I "%TARGET%"=="clean"   goto :clean
if /I "%TARGET%"=="setup"   goto :setup
if /I "%TARGET%"=="package" goto :package

call :find_vs
if errorlevel 1 exit /b 1

echo [*] Visual Studio: %FOUND_VS%
call "%FOUND_VS%" >nul
if errorlevel 1 (
    echo [!] Failed to load the Visual Studio build environment.
    exit /b 1
)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if not exist "%OBJ_ROOT%" mkdir "%OBJ_ROOT%"

rem --- Compiler flags per config ---
if /I "%CONFIG%"=="Debug" (
    set "COMMON_FLAGS=/nologo /EHsc /std:c++17 /Od /Zi /W3 /MDd /D_DEBUG"
    set "LINK_FLAGS=/INCREMENTAL:NO /DEBUG"
) else (
    set "COMMON_FLAGS=/nologo /EHsc /std:c++17 /O2 /GL /W3 /MD /DNDEBUG"
    set "LINK_FLAGS=/INCREMENTAL:NO /LTCG /OPT:REF /OPT:ICF"
)

if /I "%TARGET%"=="all" (
    call :build_dumper   || exit /b 1
    call :build_menu     || exit /b 1
    call :build_injector || exit /b 1
    goto :done
)
if /I "%TARGET%"=="dumper"   ( call :build_dumper   || exit /b 1 & goto :done )
if /I "%TARGET%"=="menu"     ( call :build_menu     || exit /b 1 & goto :done )
if /I "%TARGET%"=="injector" ( call :build_injector || exit /b 1 & goto :done )

echo [!] Unknown build target: %TARGET%
goto :usage_error

rem ================================================================
rem find_vs: locate vcvars64.bat through vswhere or fallback dirs.
rem ================================================================
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
echo [!] Install Visual Studio 2022 (or Build Tools 2022) with the
echo     "Desktop development with C++" workload, or run `build.bat setup` after installing.
exit /b 1

rem ================================================================
rem prepare_obj: per-target obj subdir under OBJ_ROOT.
rem ================================================================
:prepare_obj
set "THIS_OBJ_DIR=%OBJ_ROOT%\%~1"
if not exist "%THIS_OBJ_DIR%" mkdir "%THIS_OBJ_DIR%"
exit /b 0

rem ================================================================
rem build_dumper
rem ================================================================
:build_dumper
call :prepare_obj dumper
echo.
echo [*] Building dumper.dll...
cl %COMMON_FLAGS% /LD "%ROOT%dumper\main.cpp" /Fo"%THIS_OBJ_DIR%\\" /Fe"%OUT_DIR%\dumper.dll" /link %LINK_FLAGS%
if errorlevel 1 ( echo [!] Failed to build dumper.dll & exit /b 1 )
echo [+] %OUT_DIR%\dumper.dll
exit /b 0

rem ================================================================
rem build_injector
rem ================================================================
:build_injector
call :prepare_obj injector
echo.
echo [*] Building injector.exe...
cl %COMMON_FLAGS% ^
    /I"%ROOT%third_party\imgui" ^
    /I"%ROOT%third_party\imgui\backends" ^
    "%ROOT%injector\main.cpp" ^
    "%ROOT%injector\gui.cpp" ^
    "%ROOT%third_party\imgui\imgui.cpp" ^
    "%ROOT%third_party\imgui\imgui_draw.cpp" ^
    "%ROOT%third_party\imgui\imgui_tables.cpp" ^
    "%ROOT%third_party\imgui\imgui_widgets.cpp" ^
    "%ROOT%third_party\imgui\backends\imgui_impl_win32.cpp" ^
    "%ROOT%third_party\imgui\backends\imgui_impl_dx11.cpp" ^
    /Fo"%THIS_OBJ_DIR%\\" ^
    /Fe"%OUT_DIR%\injector.exe" ^
    /link %LINK_FLAGS% /SUBSYSTEM:WINDOWS advapi32.lib shell32.lib user32.lib d3d11.lib dxgi.lib gdi32.lib
if errorlevel 1 ( echo [!] Failed to build injector.exe & exit /b 1 )
echo [+] %OUT_DIR%\injector.exe
exit /b 0

rem ================================================================
rem build_menu (cm_hax.dll)
rem ================================================================
:build_menu
if not exist "%ROOT%third_party\imgui\imgui.h" (
    echo [!] ImGui not found at %ROOT%third_party\imgui\.
    echo [!] Run `build.bat setup` to fetch dependencies.
    exit /b 1
)
call :prepare_obj menu
echo.
echo [*] Building cm_hax.dll...
cl %COMMON_FLAGS% /LD ^
    /I"%ROOT%third_party\imgui" ^
    /I"%ROOT%third_party\imgui\backends" ^
    /I"%ROOT%src" ^
    "%ROOT%src\dllmain.cpp" ^
    "%ROOT%src\core\globals.cpp" ^
    "%ROOT%src\core\config.cpp" ^
    "%ROOT%src\core\logging.cpp" ^
    "%ROOT%src\core\hooks.cpp" ^
    "%ROOT%src\il2cpp\il2cpp.cpp" ^
    "%ROOT%src\il2cpp\player.cpp" ^
    "%ROOT%src\features\aimbot\aimbot.cpp" ^
    "%ROOT%src\features\aimbot\hitbox.cpp" ^
    "%ROOT%src\features\triggerbot\triggerbot.cpp" ^
    "%ROOT%src\features\esp.cpp" ^
    "%ROOT%src\features\combat.cpp" ^
    "%ROOT%src\features\cosmetics.cpp" ^
    "%ROOT%src\features\exploit.cpp" ^
    "%ROOT%src\features\movement.cpp" ^
    "%ROOT%src\render\menu.cpp" ^
    "%ROOT%src\render\menu_style.cpp" ^
    "%ROOT%src\render\menu_widgets.cpp" ^
    "%ROOT%src\render\esp_render.cpp" ^
    "%ROOT%src\render\overlay.cpp" ^
    "%ROOT%src\render\radar.cpp" ^
    "%ROOT%src\utils\math.cpp" ^
    "%ROOT%src\utils\memory.cpp" ^
    "%ROOT%src\utils\strings.cpp" ^
    "%ROOT%src\utils\input.cpp" ^
    "%ROOT%third_party\imgui\imgui.cpp" ^
    "%ROOT%third_party\imgui\imgui_draw.cpp" ^
    "%ROOT%third_party\imgui\imgui_tables.cpp" ^
    "%ROOT%third_party\imgui\imgui_widgets.cpp" ^
    "%ROOT%third_party\imgui\backends\imgui_impl_win32.cpp" ^
    "%ROOT%third_party\imgui\backends\imgui_impl_dx11.cpp" ^
    /Fo"%THIS_OBJ_DIR%\\" ^
    /Fe"%OUT_DIR%\cm_hax.dll" ^
    /link %LINK_FLAGS% d3d11.lib dxgi.lib user32.lib gdi32.lib
if errorlevel 1 ( echo [!] Failed to build cm_hax.dll & exit /b 1 )
echo [+] %OUT_DIR%\cm_hax.dll
exit /b 0

rem ================================================================
rem copy_sdk: keep the SDK header next to the build outputs.
rem ================================================================
:copy_sdk
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if exist "%ROOT%reference\%SDK_NAME%" (
    copy /y "%ROOT%reference\%SDK_NAME%" "%OUT_DIR%\%SDK_NAME%" >nul
    echo [+] %OUT_DIR%\%SDK_NAME%
)
exit /b 0

rem ================================================================
rem setup: fetch third_party dependencies (idempotent).
rem ================================================================
:setup
where git >nul 2>nul
if errorlevel 1 (
    echo [!] git is not on PATH. Install Git for Windows from https://git-scm.com.
    exit /b 1
)

if not exist "%ROOT%third_party" mkdir "%ROOT%third_party"

if not exist "%ROOT%third_party\imgui\imgui.h" (
    echo [*] Cloning Dear ImGui (v1.91.6) into third_party\imgui...
    git clone --depth 1 --branch v1.91.6 https://github.com/ocornut/imgui "%ROOT%third_party\imgui"
    if errorlevel 1 ( echo [!] Clone failed. & exit /b 1 )
) else (
    echo [+] third_party\imgui already present
)

echo.
echo [+] Dependencies ready. Next: run `build.bat` to build everything.
exit /b 0

rem ================================================================
rem package: build Release and produce a portable zip in release\.
rem ================================================================
:package
echo [*] Building Release artifacts...
call "%~f0" all release
if errorlevel 1 exit /b 1

if not exist "%RELEASE_ROOT%" mkdir "%RELEASE_ROOT%"

rem Build a date-stamped staging folder.
for /f %%D in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd"') do set "STAMP=%%D"
set "STAGE=%RELEASE_ROOT%\Cm-Hax_%STAMP%"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"

echo.
echo [*] Staging into %STAGE%...
copy /y "%BUILD_ROOT%\Release\cm_hax.dll"   "%STAGE%\cm_hax.dll"   >nul
copy /y "%BUILD_ROOT%\Release\dumper.dll"   "%STAGE%\dumper.dll"   >nul
copy /y "%BUILD_ROOT%\Release\injector.exe" "%STAGE%\injector.exe" >nul
if exist "%ROOT%README.md"  copy /y "%ROOT%README.md"  "%STAGE%\README.md"  >nul
if exist "%ROOT%LICENSE"    copy /y "%ROOT%LICENSE"    "%STAGE%\LICENSE"    >nul
if exist "%ROOT%LICENSE.md" copy /y "%ROOT%LICENSE.md" "%STAGE%\LICENSE.md" >nul

rem Drop a short usage note into the staged folder so the zip is self-explanatory.
> "%STAGE%\USAGE.txt" (
    echo Cm-Hax %STAMP% - portable build
    echo.
    echo Files:
    echo   injector.exe   Run as Administrator. Pick 1 ^(menu^) or 2 ^(dumper^), or pass --menu / --dump.
    echo   cm_hax.dll     Internal menu DLL. Loaded by injector option 1 / --menu.
    echo   dumper.dll     IL2CPP metadata dumper. Loaded by injector option 2 / --dump.
    echo.
    echo Notes:
    echo   - All three files must sit in the same folder.
    echo   - Config is written to %%APPDATA%%\CmHax\cm_hax.cfg.
    echo   - INSERT toggles the menu in-game; END unloads the DLL.
    echo.
    echo Disclaimer:
    echo   Educational project. Loading these into a live online game violates
    echo   Combat Master's Terms of Service and risks bans. See README.md.
)

set "ZIP_PATH=%RELEASE_ROOT%\Cm-Hax_%STAMP%.zip"
if exist "%ZIP_PATH%" del /q "%ZIP_PATH%"
echo [*] Creating %ZIP_PATH%...
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%ZIP_PATH%' -Force"
if errorlevel 1 ( echo [!] Compress-Archive failed. & exit /b 1 )

echo.
echo ============================================================
echo   PACKAGE READY
echo ============================================================
echo   %ZIP_PATH%
echo   %STAGE%\
echo.
exit /b 0

rem ================================================================
rem clean
rem ================================================================
:clean
if exist "%BUILD_ROOT%" (
    echo [*] Removing %BUILD_ROOT%...
    rmdir /s /q "%BUILD_ROOT%" 2>nul
)
if exist "%BUILD_ROOT%" (
    echo [!] Some files could not be removed ^(in use?^). Close any process holding the DLLs and retry.
)
echo [+] Clean complete.
exit /b 0

:done
call :copy_sdk
echo.
echo ============================================================
echo   BUILD COMPLETE  -  %CONFIG%
echo ============================================================
echo   Output:  %OUT_DIR%
echo   Run:     %OUT_DIR%\injector.exe
echo.
exit /b 0

:usage_error
exit /b 1

:usage
echo.
echo Cm-Hax build script
echo.
echo Usage:
echo   build.bat [target] [debug^|release]
echo.
echo Targets:
echo   all        Build dumper, menu DLL, and injector ^(default^)
echo   menu       Only build cm_hax.dll
echo   dumper     Only build dumper.dll
echo   injector   Only build injector.exe
echo   setup      Clone third_party dependencies ^(ImGui + MinHook^)
echo   package    Build Release + bundle release\Cm-Hax_^<date^>.zip
echo   clean      Remove build\
echo.
echo Configuration defaults to Release. Pass `debug` after the target for
echo a Debug build (e.g. `build.bat menu debug`).
echo.
echo First-time use:
echo   1) build.bat setup
echo   2) build.bat
echo   3) build\Release\injector.exe
echo.
exit /b 0
