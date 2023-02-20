@echo off
set succ=1
if exist dev.txt (
    echo Deleting old build...
    del /F forge.exe
)
if not exist forge.exe (
    cl /EHsc /std:c++17 forge.cpp /O2 || set succ=0
)
if %succ% == 1 (
    pushd ..
    forge\forge.exe %*
    popd
)
