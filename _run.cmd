@echo off

set APP=TestApp

rem Clean logcat...
adb logcat -c

start cmd.exe @cmd /k "mode con:cols=200 lines=50 && adb logcat -v thread san:V *:S"

rem Start app...
adb shell am start -n com.%APP%/android.app.NativeActivity
