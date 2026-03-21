//
// Created by GreenMuffin on 2025/11/22.
//

#include "window_manager.h"

#include "viewport_manager.h"

#include "runtime/core/event/event.h"
#include "runtime/core/event/event_system.h"

#include "runtime/function/input/key_code.h"
#include "runtime/function/input/mouse_code.h"

#include "GLFW/glfw3.h"


namespace dodoe {

    WindowManager::WindowManager() = default;

    WindowManager::~WindowManager() = default;

    bool WindowManager::initialize() {
        glfwInit();

        g_context.event_system->subscribe_event<WindowFocusEvent, &WindowManager::on_window_focus_>(this);
        g_context.event_system->subscribe_event<WindowCloseEvent, &WindowManager::on_window_close_>(this);
        g_context.event_system->subscribe_event<WindowResizeEvent, &WindowManager::on_window_resize_>(this);
        g_context.event_system->subscribe_event<WindowLostFocusEvent, &WindowManager::on_window_lost_focus_>(this);
        
        if (windows_.empty()) {
            WindowProperty prop;
            auto spec = Application::self().specification();
            prop.title = spec.name.c_str();
            prop.width = static_cast<int>(spec.width);
            prop.height = static_cast<int>(spec.height);
            prop.resizeable = spec.window_resizeable;
            prop.custom_titlebar = spec.custom_titlebar;
            prop.backend_api = spec.render_api_type;
            if (create_window(prop)) {
                DoInfo("Window manager initialize success.");
                return true;
            }
        }
        DoError("Window manager initialize failed.");
        return false;
    }

    void WindowManager::update() {
        for (auto& window : windows_) {
            window->swap_buffer();
        }
    }

    void WindowManager::shutdown() {
         g_context.event_system->unsubscribe_event<WindowFocusEvent, &WindowManager::on_window_focus_>(this);
         g_context.event_system->unsubscribe_event<WindowLostFocusEvent, &WindowManager::on_window_lost_focus_>(this);
         g_context.event_system->unsubscribe_event<WindowCloseEvent, &WindowManager::on_window_close_>(this);
         g_context.event_system->unsubscribe_event<WindowResizeEvent, &WindowManager::on_window_resize_>(this);

        glfwTerminate();
    }

    Window* WindowManager::active_window() const {
        return windows_.back().get();   // TODO: FIXME;
    }

    Window* WindowManager::create_window(const WindowProperty& props) {
        for (const auto& window : windows_) {
            if (window->data_.title == props.title) {
                DoError("Has the same name window.");
                return nullptr;
            }
        }
        Scope<Window> window = create_scope<Window>(props);
        if (!window->initialize()) {
            DoError("WindowManager::create_window: The window is created failure.");
            return nullptr;
        }
        bind_events_callback_(window.get());
        windows_.emplace_back(std::move(window));
        return windows_.back().get();
    }
    
    void WindowManager::bind_events_callback_(Window* window) {
        glfwSetWindowSizeCallback(window->native_window(), [](GLFWwindow* native_window, int width, int height) {
            WindowData& data = *static_cast<WindowData *>(glfwGetWindowUserPointer(native_window));
            WindowResizeEvent event(width, height, data.id);
            g_context.event_system->enqueue_event<WindowResizeEvent>(event);
        });
        glfwSetFramebufferSizeCallback(window->native_window(), [](GLFWwindow* native_window, int width, int height) {
            (void)native_window;
            (void)width;
            (void)height;
        });
        glfwSetWindowCloseCallback(window->native_window(), [](GLFWwindow* native_window) {
            WindowData& data = *static_cast<WindowData *>(glfwGetWindowUserPointer(native_window));
            WindowCloseEvent event(data.id);
            g_context.event_system->enqueue_event<WindowCloseEvent>(event);
        });
        glfwSetWindowFocusCallback(window->native_window(), [](GLFWwindow* native_window, int focused) {
            WindowData& data = *static_cast<WindowData *>(glfwGetWindowUserPointer(native_window));
            if (static_cast<bool>(focused)) {
                WindowFocusEvent event(data.id);
                g_context.event_system->enqueue_event<WindowFocusEvent>(event);
            }
            else {
                WindowLostFocusEvent event(data.id);
                g_context.event_system->enqueue_event<WindowLostFocusEvent>(event);
            }
        });
		glfwSetKeyCallback(window->native_window(), [](GLFWwindow* native_window, int key, int scancode, int action, int mods) {
			WindowData& data = *static_cast<WindowData *>(glfwGetWindowUserPointer(native_window));
			switch (action) {
				case GLFW_PRESS: {
					KeyPressedEvent event(static_cast<KeyCode>(key), false);
                    g_context.event_system->enqueue_event<KeyPressedEvent>(event);
                    break;
				}
				case GLFW_RELEASE: {
					KeyReleasedEvent event(static_cast<KeyCode>(key));
					g_context.event_system->enqueue_event<KeyReleasedEvent>(event);
					break;
				}
				case GLFW_REPEAT: {
					KeyPressedEvent event(static_cast<KeyCode>(key), true);
					g_context.event_system->enqueue_event<KeyPressedEvent>(event);
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

		glfwSetMouseButtonCallback(window->native_window(), [](GLFWwindow* native_window, int button, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(native_window);

			switch (action) {
				case GLFW_PRESS: {
					MouseButtonPressedEvent event(static_cast<MouseCode>(button));
					g_context.event_system->enqueue_event<MouseButtonPressedEvent>(event);
					break;
				}
				case GLFW_RELEASE: {
					MouseButtonReleasedEvent event(static_cast<MouseCode>(button));
					g_context.event_system->enqueue_event<MouseButtonReleasedEvent>(event);
					break;
				}
			}
		});

		glfwSetScrollCallback(window->native_window(), [](GLFWwindow* native_window, double x_offset, double y_offset)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(native_window);

			MouseScrolledEvent event(static_cast<float>(x_offset), static_cast<float>(x_offset));
			g_context.event_system->enqueue_event<MouseScrolledEvent>(event);
		});

		glfwSetCursorPosCallback(window->native_window(), [](GLFWwindow* native_window, double x_pos, double y_pos)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(native_window);

			MouseMovedEvent event(static_cast<float>(x_pos), static_cast<float>(y_pos));
			g_context.event_system->enqueue_event<MouseMovedEvent>(event);
		});
    }

     void WindowManager::on_window_focus_(const WindowFocusEvent& event) {
         if (const auto window = get_window_(event.window_id); window) {
             active_window_ = window;
         }
     }
    
     void WindowManager::on_window_lost_focus_(const WindowLostFocusEvent& event) {
         // TODO: improve here.
     }
    
     void WindowManager::on_window_close_(const WindowCloseEvent& event) {
         DoDebug("WindowManager::on_window_close");
         if (const auto window = get_window_(event.window_id); window) {
             window->shutdown();
             if (const auto it = std::ranges::find_if(windows_, [&](const Scope<Window>& w) { return w.get() == window; }); it != windows_.end()) {
                 windows_.erase(it);
             }
         }
         if (windows_.empty()) {
             DoDebug("WindowManager::application quit");
             g_context.event_system->publish_event<ApplicationQuitEvent>();
         }
     }

     void WindowManager::on_window_resize_(const WindowResizeEvent& event) {
         if (const auto window = get_window_(event.window_id); window) {
             if (const auto native_window = window->native_window()) {
                 window->prop_.width = static_cast<uint>(event.width);
                 window->prop_.height = static_cast<uint>(event.height);

                 int fb_width = 0, fb_height = 0;
                 glfwGetFramebufferSize(native_window, &fb_width, &fb_height);

                 ViewportManager::self().set_window_size(Vector2f(event.width, event.height));
                 ViewportManager::self().set_pixel_size(Vector2f(fb_width, fb_height));
             }
         }
     }

     Window* WindowManager::get_window_(const uint32_t window_id) const {
          for (const auto& window : windows_) {
              if (window->data_.id == window_id) {
                  return window.get();
              }
          }
          return nullptr;
     }

}
