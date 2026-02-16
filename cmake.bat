@echo off

mkdir _build
if defined SS_MAKE_SITE ( attrib +h _build )
cd _build
rem cmake .. -G "Visual Studio 16 2019"
cmake .. -G "Visual Studio 18 2026" -A x64
cd ..
