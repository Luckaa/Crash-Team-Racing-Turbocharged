@echo off
setlocal

:: CTR Native MSVC Build Script (Windows x86)
:: Requires Visual Studio 2022 or Build Tools with the Desktop C++ workload.

where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "PATH=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
    ) else (
        echo ERROR: cmake not found in PATH
        echo Install CMake 3.20 or newer, or the Visual Studio C++ CMake tools component.
        exit /b 1
    )
)

cmake --preset windows-msvc-x86
if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake configure failed
    exit /b 1
)

cmake --build build-msvc-x86 --config Release -t ctr_native
if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo Build succeeded: build-msvc-x86\Release\Crash Team Racing - Turbocharged.exe

