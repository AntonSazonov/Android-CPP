@echo off

set APP=TestApp
adb shell am force-stop com.%APP%.id
adb uninstall com.%APP%
