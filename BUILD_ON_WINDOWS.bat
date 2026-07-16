@echo off
setlocal
cd /d "%~dp0"
where geode >nul 2>nul || (
  echo ERROR: Geode CLI is not installed or is not on PATH.
  echo Use BUILD_INSTRUCTIONS.txt for the GitHub build method.
  pause
  exit /b 1
)
where cmake >nul 2>nul || (
  echo ERROR: CMake is not installed or is not on PATH.
  echo Use BUILD_INSTRUCTIONS.txt for the GitHub build method.
  pause
  exit /b 1
)
where ninja >nul 2>nul || (
  echo ERROR: Ninja is not installed or is not on PATH.
  echo Use BUILD_INSTRUCTIONS.txt for the GitHub build method.
  pause
  exit /b 1
)

geode sdk install || goto :fail
geode sdk install-binaries || goto :fail
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || goto :fail
cmake --build build || goto :fail

echo.
echo BUILD COMPLETE. Search the build folder for the .geode file.
pause
exit /b 0

:fail
echo.
echo BUILD FAILED. Use BUILD_INSTRUCTIONS.txt for the GitHub build method.
pause
exit /b 1