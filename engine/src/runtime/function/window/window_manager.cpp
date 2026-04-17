//
// Created by GreenMuffin on 2025/11/22.
//

#include "window_manager.h"

#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"

#include "runtime/function/input/key_code.h"
#include "runtime/function/input/mouse_code.h"

#include "GLFW/glfw3.h"


namespace dodoe {

    void WindowManager::initialize(const WindowManagerInitInfo& init_info) {
        glfwInit();

        EventSystem::subscribe_event<WindowFocusEvent, &WindowManager::onWindowFocus>(this);
        EventSystem::subscribe_event<WindowCloseEvent, &WindowManager::onWindowClose>(this);
        EventSystem::subscribe_event<WindowResizeEvent, &WindowManager::onWindowResize>(this);
        EventSystem::subscribe_event<WindowLostFocusEvent, &WindowManager::onWindowLostFocus>(this);

        WindowProperty prop;
        auto spec = init_info.spec;
        prop.title = spec.name.c_str();
        prop.width = static_cast<int>(spec.width);
        prop.height = static_cast<int>(spec.height);
        prop.resizeable = spec.window_resizeable;
        prop.custom_titlebar = spec.custom_titlebar;
        prop.backend_api = spec.render_api_type;

        window_ = Window::create(prop);
        bindEventCallback();
    }

    void WindowManager::shutdown() {
        EventSystem::unsubscribe_event<WindowFocusEvent, &WindowManager::onWindowFocus>(this);
        EventSystem::unsubscribe_event<WindowLostFocusEvent, &WindowManager::onWindowLostFocus>(this);
        EventSystem::unsubscribe_event<WindowCloseEvent, &WindowManager::onWindowClose>(this);
        EventSystem::unsubscribe_event<WindowResizeEvent, &WindowManager::onWindowResize>(this);

        Window::destroy(window_);

        glfwTerminate();
    }
    
    void WindowManager::swapBuffers() {
        window_->swapBuffers();
    }

    void WindowManager::bindEventCallback() {
        glfwSetWindowSizeCallback(window_->nativeWindow(), [](GLFWwindow* native_window, int width, int height) {
            WindowResizeEvent event(width, height);
            EventSystem::enqueueEvent<WindowResizeEvent>(event);
        });
        glfwSetFramebufferSizeCallback(window_->nativeWindow(), [](GLFWwindow* native_window, int width, int height) {
            (void)native_window;
            (void)width;
            (void)height;
        });
        glfwSetWindowCloseCallback(window_->nativeWindow(), [](GLFWwindow* native_window) {
            WindowCloseEvent event;
            EventSystem::enqueueEvent<WindowCloseEvent>(event);
        });
        glfwSetWindowFocusCallback(window_->nativeWindow(), [](GLFWwindow* native_window, int focused) {
            if (static_cast<bool>(focused)) {
                WindowFocusEvent event;
                EventSystem::enqueueEvent<WindowFocusEvent>(event);
            }
            else {
                WindowLostFocusEvent event;
                EventSystem::enqueueEvent<WindowLostFocusEvent>(event);
            }
        });
		glfwSetKeyCallback(window_->nativeWindow(), [](GLFWwindow* native_window, int key, int scancode, int action, int mods) {
			switch (action) {
				case GLFW_PRESS: {
					KeyPressedEvent event(static_cast<KeyCode>(key), false);
                    EventSystem::enqueueEvent<KeyPressedEvent>(event);
                    break;
				}
				case GLFW_RELEASE: {
					KeyReleasedEvent event(static_cast<KeyCode>(key));
					EventSystem::enqueueEvent<KeyReleasedEvent>(event);
					break;
				}
				case GLFW_REPEAT: {
					KeyPressedEvent event(static_cast<KeyCode>(key), true);
					EventSystem::enqueueEvent<KeyPressedEvent>(event);
					break;
				}
			}
		});

		// glfwSetCharCallback(window->native_window(), [](GLFWwindow* native_window, unsigned int keycode)
		// {
		// 	WindowData& data = *(WindowData*)glfwGetWindowUserPointer(native_window);

		// 	KeyTypedEvent event(keycode);
		// 	data.EventCallback(event);
		// });

		glfwSetMouseButtonCallback(window_->nativeWindow(), [](GLFWwindow* native_window, int button, int action, int mods) {
			switch (action) {
				case GLFW_PRESS: {
					MouseButtonPressedEvent event(static_cast<MouseCode>(button));
					EventSystem::enqueueEvent<MouseButtonPressedEvent>(event);
					break;
				}
				case GLFW_RELEASE: {
					MouseButtonReleasedEvent event(static_cast<MouseCode>(button));
					EventSystem::enqueueEvent<MouseButtonReleasedEvent>(event);
					break;
				}
			}
		});

		glfwSetScrollCallback(window_->nativeWindow(), [](GLFWwindow* native_window, double x_offset, double y_offset) {
			MouseScrolledEvent event(static_cast<float>(x_offset), static_cast<float>(x_offset));
			EventSystem::enqueueEvent<MouseScrolledEvent>(event);
		});

		glfwSetCursorPosCallback(window_->nativeWindow(), [](GLFWwindow* native_window, double x_pos, double y_pos) {
			MouseMovedEvent event(static_cast<float>(x_pos), static_cast<float>(y_pos));
			EventSystem::enqueueEvent<MouseMovedEvent>(event);
		});
    }

    void WindowManager::onWindowFocus(const WindowFocusEvent& event) {

    }

    void WindowManager::onWindowLostFocus(const WindowLostFocusEvent& event) {

    }

    void WindowManager::onWindowClose(const WindowCloseEvent& event) { 
        glfwSetWindowShouldClose(window_->nativeWindow(), GLFW_TRUE);
        EventSystem::publish_event<ApplicationQuitEvent>();
    }

    void WindowManager::onWindowResize(const WindowResizeEvent& event) {
        window_->prop_.width = static_cast<uint>(event.width);
        window_->prop_.height = static_cast<uint>(event.height);

        int fb_width = 0, fb_height = 0;
        glfwGetFramebufferSize(window_->nativeWindow(), &fb_width, &fb_height);
    }

}
