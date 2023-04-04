cmake_minimum_required( VERSION 3.7 )

# Check for App name...
if( "${APP}" STREQUAL "" )
 message( FATAL_ERROR "APP is not set. Please set App name with '-DAPP=AppName' cmake parameter." )
endif()

# Check for ABI...
if( "${ABI}" STREQUAL "" )
 message( FATAL_ERROR "ABI is not set. Please set ABI with '-DABI=armeabi-v7a' cmake parameter." )
endif()
set( CMAKE_ANDROID_ARCH_ABI ${ABI} )

# Check for API level...
if( NOT DEFINED API )
 message( FATAL_ERROR "API is not set. Please set API level with '-DAPI=21' cmake parameter." )
endif()
set( CMAKE_SYSTEM_VERSION ${API} )


# Check for NDK...
file( TO_CMAKE_PATH "$ENV{ANDROID_NDK_HOME}" CMAKE_ANDROID_NDK )
if( "${CMAKE_ANDROID_NDK}" STREQUAL "" )
 message( FATAL_ERROR "ANDROID_NDK_HOME environment variable is not set." )
endif()

# Check for SDK...
file( TO_CMAKE_PATH "$ENV{ANDROID_HOME}" SDK )
if( "${SDK}" STREQUAL "" )
 message( FATAL_ERROR "ANDROID_HOME environment variable is not set." )
endif()

set( KEYSTORE_FILE "my-test-keystore.jks" )
set( KEYSTORE_PASS "123456" )

set( JAR "${SDK}/platforms/android-${API}/android.jar" )

# Set defaults...
set( CMAKE_SYSTEM_NAME Android )
set( CMAKE_ANDROID_STL_TYPE c++_static )
#set( CMAKE_ANDROID_ARM_NEON ON )
#set( CMAKE_CXX_STANDARD 17 )
#set( CMAKE_CXX_STANDARD_REQUIRED ON )

project( ${APP} )

set( COMMON_FLAGS "-fomit-frame-pointer -fvisibility=hidden" ) #  -fno-rtti -flto
set( CMAKE_C_FLAGS "${COMMON_FLAGS}" )
set( CMAKE_CXX_FLAGS "${COMMON_FLAGS} -std=c++17 -fno-exceptions" ) #

add_library( ${CMAKE_PROJECT_NAME} SHARED
	san_activity.hpp
	android_native_test_app.cpp )

target_include_directories( ${CMAKE_PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR} )
target_link_options       ( ${CMAKE_PROJECT_NAME} PRIVATE -s )
target_link_libraries     ( ${CMAKE_PROJECT_NAME} PRIVATE android log )
set_target_properties     ( ${CMAKE_PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${CMAKE_ANDROID_ARCH_ABI}" )


# Process manifest...
set( MANIFEST ${CMAKE_BINARY_DIR}/AndroidManifest.xml )
if( NOT EXISTS ${MANIFEST} )
 message( STATUS "Generating ${MANIFEST}..." )
 configure_file( ${CMAKE_SOURCE_DIR}/AndroidManifest.xml.in ${MANIFEST} )
endif()

set( APK_DIR ${CMAKE_BINARY_DIR} )
set( APK ${APK_DIR}/${CMAKE_PROJECT_NAME} )

find_program( EXE_READELF llvm-readelf )
if( NOT DEFINED EXE_READELF )
 message( ERROR "readelf not found." )
endif()

# Generate keystore...
add_custom_command( OUTPUT ${KEYSTORE_FILE}
	COMMAND keytool -genkey -keystore ${KEYSTORE_FILE} -keypass ${KEYSTORE_PASS} -storepass ${KEYSTORE_PASS} -keyalg RSA -keysize 2048 -validity 10000 -dname "C=RU"
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	VERBATIM
)

# Keystore target
add_custom_target( keystore DEPENDS ${KEYSTORE_FILE} )


# Pack, align and sign...
add_custom_target( apk
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/res
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/assets
	COMMAND aapt package --no-crunch -f -0 apk -M ${MANIFEST} -I ${JAR} -S ${CMAKE_SOURCE_DIR}/res -A ${CMAKE_SOURCE_DIR}/assets -F ${APK}.apk --generate-dependencies
	COMMAND ${EXE_READELF} --dynamic lib/${CMAKE_ANDROID_ARCH_ABI}/$<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}> | grep NEEDED
	COMMAND aapt a ${APK}.apk lib/${CMAKE_ANDROID_ARCH_ABI}/$<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}>
	COMMAND zipalign -f -v -p 4 ${APK}.apk ${APK}-aligned.apk
	COMMAND apksigner.bat sign --ks ${KEYSTORE_FILE} --ks-pass pass:${KEYSTORE_PASS} ${APK}-aligned.apk
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_dependencies( apk keystore )
