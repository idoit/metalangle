//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "SampleApplication.h"

#include "util/EGLWindow.h"
#include "util/gles_loader_autogen.h"
#include "util/random_utils.h"
#include "util/test_utils.h"

#include <string.h>
#include <iostream>
#include <utility>

// Use environment variable if this variable is not compile time defined.
#if !defined(ANGLE_EGL_LIBRARY_NAME)
#    define ANGLE_EGL_LIBRARY_NAME angle::GetEnvironmentVar("ANGLE_EGL_LIBRARY_NAME").c_str()
#endif

namespace
{
const char *kUseAngleArg = "--use-angle=";

using DisplayTypeInfo = std::pair<const char *, EGLint>;

const DisplayTypeInfo kDisplayTypes[] = {
    {"d3d9", EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE},
    {"d3d11", EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE},
    {"gl", EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE},
    {"gles", EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE},
    {"null", EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE},
    {"vulkan", EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE},
    {"swiftshader", EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE},
};

EGLint GetDisplayTypeFromArg(const char *displayTypeArg)
{
    for (const auto &displayTypeInfo : kDisplayTypes)
    {
        if (strcmp(displayTypeInfo.first, displayTypeArg) == 0)
        {
            std::cout << "Using ANGLE back-end API: " << displayTypeInfo.first << std::endl;
            return displayTypeInfo.second;
        }
    }

    std::cout << "Unknown ANGLE back-end API: " << displayTypeArg << std::endl;
    return EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE;
}

EGLint GetDeviceTypeFromArg(const char *displayTypeArg)
{
    if (strcmp(displayTypeArg, "swiftshader") == 0)
    {
        return EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE;
    }
    else
    {
        return EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE;
    }
}
}  // anonymous namespace

SampleApplication::SampleApplication(std::string name,
                                     int argc,
                                     char **argv,
                                     EGLint glesMajorVersion,
                                     EGLint glesMinorVersion,
                                     uint32_t width,
                                     uint32_t height)
    : mName(std::move(name)),
      mWidth(width),
      mHeight(height),
      mRunning(false),
      mEGLWindow(nullptr),
      mOSWindow(nullptr)
{
    mPlatformParams.renderer = EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE;

    if (argc > 1 && strncmp(argv[1], kUseAngleArg, strlen(kUseAngleArg)) == 0)
    {
        const char *arg            = argv[1] + strlen(kUseAngleArg);
        mPlatformParams.renderer   = GetDisplayTypeFromArg(arg);
        mPlatformParams.deviceType = GetDeviceTypeFromArg(arg);
    }

    // Load EGL library so we can initialize the display.
    mEntryPointsLib.reset(
        angle::OpenSharedLibrary(ANGLE_EGL_LIBRARY_NAME, angle::SearchType::ApplicationDir));

    mEGLWindow = EGLWindow::New(glesMajorVersion, glesMinorVersion);
    mOSWindow  = OSWindow::New();
}

SampleApplication::~SampleApplication()
{
    EGLWindow::Delete(&mEGLWindow);
    OSWindow::Delete(&mOSWindow);
}

bool SampleApplication::initialize()
{
    return true;
}

void SampleApplication::destroy() {}

void SampleApplication::step(float dt, double totalTime) {}

void SampleApplication::draw() {}

void SampleApplication::swap()
{
    mEGLWindow->swap();
}

OSWindow *SampleApplication::getWindow() const
{
    return mOSWindow;
}

EGLConfig SampleApplication::getConfig() const
{
    return mEGLWindow->getConfig();
}

EGLDisplay SampleApplication::getDisplay() const
{
    return mEGLWindow->getDisplay();
}

EGLSurface SampleApplication::getSurface() const
{
    return mEGLWindow->getSurface();
}

EGLContext SampleApplication::getContext() const
{
    return mEGLWindow->getContext();
}

int SampleApplication::prepareToRun()
{
    mOSWindow->setVisible(true);

    ConfigParameters configParams;
    configParams.redBits     = 8;
    configParams.greenBits   = 8;
    configParams.blueBits    = 8;
    configParams.alphaBits   = 8;
    configParams.depthBits   = 24;
    configParams.stencilBits = 8;

    if (!mEGLWindow->initializeGL(mOSWindow, mEntryPointsLib.get(), mPlatformParams, configParams))
    {
        return -1;
    }

    // Disable vsync
    if (!mEGLWindow->setSwapInterval(0))
    {
        return -1;
    }

    angle::LoadGLES(eglGetProcAddress);

    mRunning = true;

    if (!initialize())
    {
        mRunning = false;
        return -1;
    }

    mTimer.start();
    mPrevTime = 0.0;

    return 0;
}

int SampleApplication::runIteration()
{
    double elapsedTime = mTimer.getElapsedTime();
    double deltaTime   = elapsedTime - mPrevTime;

    step(static_cast<float>(deltaTime), elapsedTime);

    // Clear events that the application did not process from this frame
    Event event;
    while (popEvent(&event))
    {
        // If the application did not catch a close event, close now
        if (event.Type == Event::EVENT_CLOSED)
        {
            exit();
        }
    }

    if (!mRunning)
    {
        return 0;
    }

    draw();
    swap();

    mOSWindow->messageLoop();

    mPrevTime = elapsedTime;

    return 0;
}

int SampleApplication::run()
{
    if (!mOSWindow->initialize(mName, mWidth, mHeight))
    {
        return -1;
    }

    int result = 0;
    if (mOSWindow->hasOwnLoop())
    {
        // The Window platform has its own message loop, so let it run its own
        // using our delegates.
        result = mOSWindow->runOwnLoop([this] { return prepareToRun(); },
                                       [this] { return runIteration(); });
    }
    else
    {
        if ((result = prepareToRun()))
        {
            return result;
        }

        while (mRunning)
        {
            runIteration();
        }
    }

    destroy();
    mEGLWindow->destroyGL();
    mOSWindow->destroy();

    return result;
}

void SampleApplication::exit()
{
    mRunning = false;
}

bool SampleApplication::popEvent(Event *event)
{
    return mOSWindow->popEvent(event);
}
