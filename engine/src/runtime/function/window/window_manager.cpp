//
// Created by GreenMuffin on 2025/11/22.
//

#include "window_manager.h"

#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"

#include "runtime/function/input/key_code.h"
#include "runtime/function/input/mouse_code.h"

#include "GLFW/glfw3.h"

#ifdef DODOE_EDITOR
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#endif


namespace dodoe {

    void WindowManager::initialize(const WindowManagerInitInfo& init_info) {
        glfwInit();

        EventSystem::Subscribe<WindowFocusEvent, &WindowManager::onWindowFocus>(this);
        EventSystem::Subscribe<WindowCloseEvent, &WindowManager::onWindowClose>(this);
        EventSystem::Subscribe<WindowResizeEvent, &WindowManager::onWindowResize>(this);
        EventSystem::Subscribe<WindowLostFocusEvent, &WindowManager::onWindowLostFocus>(this);

        WindowProperty prop;
        auto spec = init_info.spec;
        prop.title = spec.name.c_str();
        prop.width = static_cast<int>(spec.width);
        prop.height = static_cast<int>(spec.height);
        prop.resizeable = spec.window_resizeable;
        prop.custom_titlebar = spec.custom_titlebar;
        prop.backend_api = spec.render_settings.api;

        window_ = Window::Create(prop);
        bindEventCallback();
    }

    void WindowManager::shutdown() {
        EventSystem::Unsubscribe<WindowFocusEvent, &WindowManager::onWindowFocus>(this);
        EventSystem::Unsubscribe<WindowLostFocusEvent, &WindowManager::onWindowLostFocus>(this);
        EventSystem::Unsubscribe<WindowCloseEvent, &WindowManager::onWindowClose>(this);
        EventSystem::Unsubscribe<WindowResizeEvent, &WindowManager::onWindowResize>(this);

        Window::Destroy(window_);

        glfwTerminate();
    }
    
    void WindowManager::swapBuffers() {
        window_->swapBuffers();
    }

    void WindowManager::bindEventCallback() {
        glfwSetWindowSizeCallback(window_->getNativeWindow(), [](GLFWwindow* native_window, int width, int height) {
            WindowResizeEvent event(width, height);
            EventSystem::Enqueue<WindowResizeEvent>(event);
        });
        glfwSetFramebufferSizeCallback(window_->getNativeWindow(), [](GLFWwindow* native_window, int width, int height) {
            (void)native_window;
            (void)width;
            (void)height;
        });
        glfwSetWindowCloseCallback(window_->getNativeWindow(), [](GLFWwindow* native_window) {
            WindowCloseEvent event;
            EventSystem::Enqueue<WindowCloseEvent>(event);
        });
        glfwSetWindowFocusCallback(window_->getNativeWindow(), [](GLFWwindow* native_window, int focused) {
            if (static_cast<bool>(focused)) {
                WindowFocusEvent event;
                EventSystem::Enqueue<WindowFocusEvent>(event);
            }
            else {
                WindowLostFocusEvent event;
                EventSystem::Enqueue<WindowLostFocusEvent>(event);
            }
        });
		glfwSetKeyCallback(window_->getNativeWindow(), [](GLFWwindow* native_window, int key, int scancode, int action, int mods) {
#ifdef DODOE_EDITOR
            if (ImGui::GetCurrentContext()) {
                ImGui_ImplGlfw_KeyCallback(native_window, key, scancode, action, mods);
            }
#endif
			switch (action) {
				case GLFW_PRESS: {
					KeyPressedEvent event(static_cast<KeyCode>(key), false);
                    EventSystem::Enqueue<KeyPressedEvent>(event);
                    break;
				}
				case GLFW_RELEASE: {
					KeyReleasedEvent event(static_cast<KeyCode>(key));
					EventSystem::Enqueue<KeyReleasedEvent>(event);
					break;
				}
				case GLFW_REPEAT: {
					KeyPressedEvent event(static_cast<KeyCode>(key), true);
					EventSystem::Enqueue<KeyPressedEvent>(event);
					break;
				}
			}
		});

		glfwSetCharCallback(window_->getNativeWindow(), [](GLFWwindow* native_window, unsigned int keycode) {
#ifdef DODOE_EDITOR
            if (ImGui::GetCurrentContext()) {
                ImGui_ImplGlfw_CharCallback(native_window, keycode);
            }
#endif
		});

		glfwSetMouseButtonCallback(window_->getNativeWindow(), [](GLFWwindow* native_window, int button, int action, int mods) {
#ifdef DODOE_EDITOR
            if (ImGui::GetCurrentContext()) {
                ImGui_ImplGlfw_MouseButtonCallback(native_window, button, action, mods);
            }
#endif
			switch (action) {
				case GLFW_PRESS: {
					MouseButtonPressedEvent event(static_cast<MouseCode>(button));
					EventSystem::Enqueue<MouseButtonPressedEvent>(event);
					break;
				}
				case GLFW_RELEASE: {
					MouseButtonReleasedEvent event(static_cast<MouseCode>(button));
					EventSystem::Enqueue<MouseButtonReleasedEvent>(event);
					break;
				}
			}
		});

		glfwSetScrollCallback(window_->getNativeWindow(), [](GLFWwindow* native_window, double x_offset, double y_offset) {
#ifdef DODOE_EDITOR
            if (ImGui::GetCurrentContext()) {
                ImGui_ImplGlfw_ScrollCallback(native_window, x_offset, y_offset);
            }
#endif
			MouseScrolledEvent event(static_cast<float>(x_offset), static_cast<float>(y_offset));
			EventSystem::Enqueue<MouseScrolledEvent>(event);
		});

		glfwSetCursorPosCallback(window_->getNativeWindow(), [](GLFWwindow* native_window, double x_pos, double y_pos) {
#ifdef DODOE_EDITOR
            if (ImGui::GetCurrentContext()) {
                ImGui_ImplGlfw_CursorPosCallback(native_window, x_pos, y_pos);
            }
#endif
			MouseMovedEvent event(static_cast<float>(x_pos), static_cast<float>(y_pos));
			EventSystem::Enqueue<MouseMovedEvent>(event);
		});
    }

    void WindowManager::onWindowFocus(const WindowFocusEvent& event) {

    }

    void WindowManager::onWindowLostFocus(const WindowLostFocusEvent& event) {

    }

    void WindowManager::onWindowClose(const WindowCloseEvent& event) { 
        glfwSetWindowShouldClose(window_->getNativeWindow(), GLFW_TRUE);
        EventSystem::Publish<ApplicationQuitEvent>();
    }

    void WindowManager::onWindowResize(const WindowResizeEvent& event) {
        window_->prop_.width = static_cast<uint>(event.width);
        window_->prop_.height = static_cast<uint>(event.height);

        int fb_width = 0, fb_height = 0;
        glfwGetFramebufferSize(window_->getNativeWindow(), &fb_width, &fb_height);
    }

}
