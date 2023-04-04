@echo off
setlocal
chcp 65001
cls
echo.

set APP=TestApp

rem arm64-v8a,x86,x86_64
set ABI=armeabi-v7a

set API=21
set BLD=build
set BUILD_TYPE=Debug

rem Build...
cmake -G Ninja -B %BLD%/%ABI% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DAPP=%APP% -DAPI=%API% -DABI=%ABI% && ^
cmake --build %BLD%/%ABI% && ^
cmake --build %BLD%/%ABI% --target apk

rem Install...
adb install -r %BLD%/%ABI%/%APP%-aligned.apk
