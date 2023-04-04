@echo off

set APP=TestApp

adb logcat -c
start cmd.exe @cmd /k "adb logcat -b crash"
adb shell am start -n com.%APP%/android.app.NativeActivity
