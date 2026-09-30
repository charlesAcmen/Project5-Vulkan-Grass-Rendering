#include <stdio.h>
#include "Window.h"

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <imm.h>
#endif

namespace {
    GLFWwindow* window = nullptr;

#ifdef _WIN32
    constexpr wchar_t kEnglishKeyboardLayout[] = L"00000409";

    void RequestEnglishInputMode(GLFWwindow* glfwWindow) {
        HWND nativeWindow = glfwGetWin32Window(glfwWindow);
        HKL englishLayout = LoadKeyboardLayoutW(kEnglishKeyboardLayout, KLF_ACTIVATE);
        if (englishLayout != nullptr) {
            // Activate the layout on GLFW's window thread and ask the native
            // window to accept the same input-language change.
            ActivateKeyboardLayout(englishLayout, 0);
            SendMessageW(nativeWindow, WM_INPUTLANGCHANGEREQUEST, 0,
                reinterpret_cast<LPARAM>(englishLayout));
        }

        HIMC inputContext = ImmGetContext(nativeWindow);
        if (inputContext == nullptr) {
            return;
        }

        // Also close any IME composition mode retained specifically for this
        // window. Layout activation and IME conversion state are independent.
        ImmSetConversionStatus(inputContext, IME_CMODE_ALPHANUMERIC, IME_SMODE_NONE);
        ImmSetOpenStatus(inputContext, FALSE);
        ImmReleaseContext(nativeWindow, inputContext);
    }

    void WindowFocusCallback(GLFWwindow* glfwWindow, int focused) {
        if (focused == GLFW_TRUE) {
            RequestEnglishInputMode(glfwWindow);
        }
    }
#endif
}

GLFWwindow* GetGLFWWindow() {
    return window;
}

void InitializeWindow(int width, int height, const char* name) {
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        exit(EXIT_FAILURE);
    }

    if (!glfwVulkanSupported()){
        fprintf(stderr, "Vulkan not supported\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, name, nullptr, nullptr);

    if (!window) {
        fprintf(stderr, "Failed to initialize GLFW window\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

#ifdef _WIN32
    glfwSetWindowFocusCallback(window, WindowFocusCallback);
    RequestEnglishInputMode(window);
#endif
}

bool ShouldQuit() {
    return !!glfwWindowShouldClose(window);
}

void DestroyWindow() {
    glfwDestroyWindow(window);
    glfwTerminate();
}
