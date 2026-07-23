cmake_minimum_required(VERSION 3.21)

# Must be set before project(): the broadest macOS range Qt6's own Homebrew
# formula realistically supports. Not literally "every macOS version ever"
# (Qt6 itself dropped anything older years ago) but this is the practical
# floor — Big Sur (2020) and newer.
if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(MicroscopeLab VERSION 2.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# One codebase, one feature set: the edition (Scolaire / Grand public /
# Laboratoire) is chosen AT RUNTIME on first launch and adjustable in the
# settings — no compile-time FEATURE variants anymore (they used to exist as
# E_LAB_PUBLIC_BUILD / E_LAB_INDIVIDUAL_BUILD options; the runtime feature
# flags in AppSettings replaced them).
#
# The only thing still chosen at compile time is BRANDING (logo/icon/wizard
# images can't be swapped at runtime — they're baked into the .exe via qrc/
# .rc). Two branding builds are produced from this same source:
#   cmake -S . -B build                                   (generic/open-source, default)
#   cmake -S . -B build-ruelle -DE_LAB_SCHOOL_BRANDING=ON  (Ruelle de l'avenir, private)
option(E_LAB_SCHOOL_BRANDING "Use the Ruelle de l'avenir logo/icon instead of the generic open-source ones" OFF)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Concurrent)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc videoio)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
# UvcCameraBackend uses only OpenCV's videoio (cv::CAP_DSHOW on Windows,
# cv::CAP_AVFOUNDATION on macOS), so the real camera backend needs no
# proprietary SDK, header, or DLL at all — see README.md > Caméra du
# microscope for why (the ToupCam SDK's own device enumeration doesn't see
# this camera, but the OS exposes it as a standard UVC capture device once
# its driver — Windows only — is installed; macOS needs no separate driver).
#
# MailSender has one implementation per platform (Windows: Simple MAPI,
# macOS: NSSharingService) behind the same free-function signature.
if(WIN32)
    set(MAIL_SENDER_SOURCE src/core/MailSender.cpp)
elseif(APPLE)
    set(MAIL_SENDER_SOURCE src/core/MailSender_mac.mm)
endif()

set(CORE_SOURCES
    src/core/CameraTypes.h
    src/core/CameraBackend.h
    src/core/NullCameraBackend.h
    src/core/NullCameraBackend.cpp
    src/core/UvcCameraBackend.h
    src/core/UvcCameraBackend.cpp
    src/core/CameraManager.h
    src/core/CameraManager.cpp
    src/core/CaptureManager.h
    src/core/CaptureManager.cpp
    src/core/GalleryModel.h
    src/core/GalleryModel.cpp
    src/core/AppSettings.h
    src/core/AppSettings.cpp
    src/core/MailSender.h
    ${MAIL_SENDER_SOURCE}
    src/core/FileUtils.h
    src/core/FileUtils.cpp
)

set(UI_SOURCES
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/VideoView.h
    src/ui/VideoView.cpp
    src/ui/BottomBar.h
    src/ui/BottomBar.cpp
    src/ui/TopStatusBar.h
    src/ui/TopStatusBar.cpp
    src/ui/TeacherPanel.h
    src/ui/TeacherPanel.cpp
    src/ui/GalleryDialog.h
    src/ui/GalleryDialog.cpp
    src/ui/IdleScreen.h
    src/ui/IdleScreen.cpp
    src/ui/SplashScreen.h
    src/ui/SplashScreen.cpp
    src/ui/StartupSelectionDialog.h
    src/ui/StartupSelectionDialog.cpp
    src/ui/ClassManagerDialog.h
    src/ui/ClassManagerDialog.cpp
    src/ui/SmoothScrollArea.h
    src/ui/SmoothScrollArea.cpp
    src/ui/MicroscopeInfoPanel.h
    src/ui/MicroscopeInfoPanel.cpp
    src/ui/PhotoEditorDialog.h
    src/ui/PhotoEditorDialog.cpp
    src/ui/ComparisonDialog.h
    src/ui/ComparisonDialog.cpp
    src/ui/TrashDialog.h
    src/ui/TrashDialog.cpp
    src/ui/ScaleCalibrationDialog.h
    src/ui/ScaleCalibrationDialog.cpp
    src/ui/HelpDialog.h
    src/ui/HelpDialog.cpp
    src/ui/TeacherOverviewDialog.h
    src/ui/TeacherOverviewDialog.cpp
    src/ui/MicroscopeDiagramDialog.h
    src/ui/MicroscopeDiagramDialog.cpp
    src/ui/GlossaryDialog.h
    src/ui/GlossaryDialog.cpp
    src/ui/AnalysisToolsDialog.h
    src/ui/AnalysisToolsDialog.cpp
    src/ui/ModeSelectionDialog.h
    src/ui/ModeSelectionDialog.cpp
)

if(E_LAB_SCHOOL_BRANDING)
    set(BRANDING_QRC resources/resources.qrc)
else()
    set(BRANDING_QRC resources/resources_public.qrc)
endif()

set(APP_ICON_RC "")
if(WIN32)
    if(E_LAB_SCHOOL_BRANDING)
        set(APP_ICON_RC resources/app.rc)
    else()
        set(APP_ICON_RC resources/app_public.rc)
    endif()
endif()

# macOS app bundle icon: a real .icns file, produced by `iconutil` (a macOS-
# only tool) from resources/mac_iconset_<school|public>.iconset — see
# .github/workflows/macos-build.yml, which runs iconutil before configuring
# CMake. Not generated by CMake itself since iconutil doesn't exist off-mac.
set(APP_ICON_ICNS "")
if(APPLE)
    if(E_LAB_SCHOOL_BRANDING)
        set(APP_ICON_ICNS "${CMAKE_SOURCE_DIR}/resources/AppIcon_ruelle.icns")
    else()
        set(APP_ICON_ICNS "${CMAKE_SOURCE_DIR}/resources/AppIcon_public.icns")
    endif()
    if(NOT EXISTS "${APP_ICON_ICNS}")
        message(WARNING "macOS .icns not found at ${APP_ICON_ICNS} — run iconutil first "
                         "(see .github/workflows/macos-build.yml); building without a custom icon.")
        set(APP_ICON_ICNS "")
    endif()
endif()

add_executable(MicroscopeLab WIN32 MACOSX_BUNDLE
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${BRANDING_QRC}
    ${APP_ICON_RC}
    ${APP_ICON_ICNS}
)

set_target_properties(MicroscopeLab PROPERTIES OUTPUT_NAME "E-Lab700")

if(APPLE AND APP_ICON_ICNS)
    set_source_files_properties(${APP_ICON_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

if(APPLE)
    # Info.plist.in uses @ELAB_...@ placeholders, filled in here (plain
    # configure_file substitution, not CMake's automatic MACOSX_BUNDLE_*
    # mechanism, so the two brandings can produce genuinely different
    # bundle identifiers from the same template).
    set(ELAB_BUNDLE_EXECUTABLE "E-Lab700")
    set(ELAB_BUNDLE_NAME "E-Lab 700")
    set(ELAB_BUNDLE_VERSION "${PROJECT_VERSION}")
    set(ELAB_OSX_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    if(APP_ICON_ICNS)
        get_filename_component(ELAB_BUNDLE_ICON_FILE "${APP_ICON_ICNS}" NAME)
    else()
        set(ELAB_BUNDLE_ICON_FILE "")
    endif()
    if(E_LAB_SCHOOL_BRANDING)
        set(ELAB_BUNDLE_IDENTIFIER "org.ruelledelavenir.elab700")
    else()
        set(ELAB_BUNDLE_IDENTIFIER "org.elab700community.elab700")
    endif()
    configure_file("${CMAKE_SOURCE_DIR}/resources/Info.plist.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)
    set_target_properties(MicroscopeLab PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
endif()

target_include_directories(MicroscopeLab PRIVATE src)

if(E_LAB_SCHOOL_BRANDING)
    target_compile_definitions(MicroscopeLab PRIVATE E_LAB_SCHOOL_BRANDING)
endif()

if(APPLE)
    # MailSender_mac.mm is Objective-C++ and uses ARC (no manual
    # retain/release) — the rest of the project stays plain C++17.
    set_source_files_properties(src/core/MailSender_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
endif()

target_link_libraries(MicroscopeLab PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Concurrent
    ${OpenCV_LIBS}
)

if(APPLE)
    target_link_libraries(MicroscopeLab PRIVATE ${APPKIT_FRAMEWORK})
endif()

install(TARGETS MicroscopeLab
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .)
