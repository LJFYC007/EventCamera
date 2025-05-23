/***************************************************************************
 # Copyright (c) 2015-23, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#pragma once
#include "Macros.h"
#include "Object.h"
#include "Platform/PlatformHandles.h"
#include "Utils/Math/Vector.h"
#include <memory>
#include <string>
#include <filesystem>

struct GLFWwindow;

namespace Falcor
{
struct KeyboardEvent;
struct MouseEvent;
struct GamepadEvent;
struct GamepadState;

class FALCOR_API Window : public Object
{
    FALCOR_OBJECT(Window)
public:
    using ApiHandle = WindowHandle;

    /**
     * Window mode
     */
    enum class WindowMode
    {
        Normal,     ///< Normal window.
        Minimized,  ///< Minimized window.
        Fullscreen, ///< Fullscreen window.
    };

    /**
     * Window configuration configuration
     */
    struct Desc
    {
        uint32_t width = 1280;                ///< The width of the client area size.
        uint32_t height = 720;               ///< The height of the client area size.
        std::string title = "Falcor Sample";  ///< Window title.
        WindowMode mode = WindowMode::Normal; ///< Window mode. In full screen mode, width and height will be ignored.
        bool resizableWindow = true;          ///< Allow the user to resize the window.
        bool enableVSync = false;             ///< Controls vertical-sync.
    };

    /**
     * Callbacks interface to be used when creating a new object
     */
    class ICallbacks
    {
    public:
        virtual void handleWindowSizeChange() = 0;
        virtual void handleRenderFrame() = 0;
        virtual void handleKeyboardEvent(const KeyboardEvent& keyEvent) = 0;
        virtual void handleMouseEvent(const MouseEvent& mouseEvent) = 0;
        virtual void handleGamepadEvent(const GamepadEvent& gamepadEvent) = 0;
        virtual void handleGamepadState(const GamepadState& gamepadState) = 0;
        virtual void handleDroppedFile(const std::filesystem::path& path) = 0;
    };

    /**
     * Create a new window.
     * @param[in] desc Window configuration
     * @param[in] pCallbacks User callbacks
     * @return A new object, or throws an exception if creation failed.
     */
    static ref<Window> create(const Desc& desc, ICallbacks* pCallbacks);

    /**
     * Destructor
     */
    ~Window();

    /**
     * Destroy the window. This will cause the msgLoop() to stop its execution
     */
    void shutdown();

    /**
     * Returns true if the window should close.
     */
    bool shouldClose() const;

    /**
     * Resize the window
     * @param[in] width The new width of the client-area
     * @param[in] height The new height of the client-area
     * There is not guarantee that the call will succeed. You should call getClientAreaHeight() and getClientAreaWidth() to get the actual
     * new size of the window
     */
    void resize(uint32_t width, uint32_t height);

    /**
     * Start executing the msgLoop. The only way to stop it is to call shutdown()
     */
    void msgLoop();

    /**
     * Force event polling. Useful if your rendering loop is slow and you would like to get a recent keyboard/mouse status
     */
    void pollForEvents();

    /**
     * Handle gamepad input.
     */
    void handleGamepadInput();

    /**
     * Change the window's position
     */
    void setWindowPos(int32_t x, int32_t y);

    /**
     * Change the window's title
     */
    void setWindowTitle(const std::string& title);

    /**
     * Change the window's icon
     */
    void setWindowIcon(const std::filesystem::path& path);

    /**
     * Get the native window handle
     */
    const ApiHandle& getApiHandle() const { return mApiHandle; }

    /**
     * Get the width of the window's client area
     */
    uint2 getClientAreaSize() const { return {mDesc.width, mDesc.height}; }

    /**
     * Get the descriptor
     */
    const Desc& getDesc() const { return mDesc; }

private:
    friend class ApiCallbacks;
    Window(const Desc& desc, ICallbacks* pCallbacks);

    void updateWindowSize();
    void setWindowSize(uint32_t width, uint32_t height);

    Desc mDesc;
    GLFWwindow* mpGLFWWindow;
    ApiHandle mApiHandle;
    float2 mMouseScale;
    const float2& getMouseScale() const { return mMouseScale; }
    ICallbacks* mpCallbacks = nullptr;

    struct GamepadData;
    std::unique_ptr<GamepadData> mpGamepadData;
};
} // namespace Falcor
