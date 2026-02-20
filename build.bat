@echo off

mkdir _build
if defined SS_MAKE_SITE ( attrib +h _build )
rem cd _build
rem cmake .. -G "Visual Studio 16 2019"
rem cmake .. -G "Visual Studio 18 2026" -A x64
cmake -S . -B _build -G "Visual Studio 18 2026" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=d:/WORK/external/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
rem cd ..
