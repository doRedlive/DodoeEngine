// do@Redlive

#pragma once

#include "dopch.h"

namespace dodoe {

    class IViewExtension {
    public:
        virtual ~IViewExtension() = default;
        virtual void reset() = 0;
    };

    class ViewExtensionContainer {
        UnorderedMap<Size_t, Scope<IViewExtension>> m_extensions{};

    public:
        ViewExtensionContainer() = default;
        ~ViewExtensionContainer() = default;
        ViewExtensionContainer(const ViewExtensionContainer&) = delete;
        ViewExtensionContainer& operator=(const ViewExtensionContainer&) = delete;
        ViewExtensionContainer(ViewExtensionContainer&&) = default;
        ViewExtensionContainer& operator=(ViewExtensionContainer&&) = default;

    public:
        template <typename TExtension, typename... TArgs>
        TExtension& getOrCreate(TArgs&&... args) {
            static_assert(std::is_base_of_v<IViewExtension, TExtension>, "TExtension must inherit from IViewExtension");

            const Size_t type_id = typeid(TExtension).hash_code();
            auto it = m_extensions.find(type_id);

            if (it == m_extensions.end()) {
                auto extension = create_scope<TExtension>(std::forward<TArgs>(args)...);
                auto* ptr = extension.get();
                m_extensions.emplace(type_id, std::move(extension));
                return *ptr;
            }

            return *static_cast<TExtension*>(it->second.get());
        }

        template <typename TExtension>
        TExtension* get() {
            static_assert(std::is_base_of_v<IViewExtension, TExtension>, "TExtension must inherit from IViewExtension");

            const Size_t type_id = typeid(TExtension).hash_code();
            auto it = m_extensions.find(type_id);

            return it != m_extensions.end() ? static_cast<TExtension*>(it->second.get()) : nullptr;
        }

        template <typename TExtension>
        const TExtension* get() const {
            static_assert(std::is_base_of_v<IViewExtension, TExtension>, "TExtension must inherit from IViewExtension");

            const Size_t type_id = typeid(TExtension).hash_code();
            auto it = m_extensions.find(type_id);

            return it != m_extensions.end() ? static_cast<const TExtension*>(it->second.get()) : nullptr;
        }

        template <typename TExtension>
        Bool has() const {
            static_assert(std::is_base_of_v<IViewExtension, TExtension>, "TExtension must inherit from IViewExtension");
            return m_extensions.find(typeid(TExtension).hash_code()) != m_extensions.end();
        }

        void reset() {
            for (auto& [type_id, extension] : m_extensions) {
                extension->reset();
            }
        }

        void clear() {
            m_extensions.clear();
        }
    };

} // dodoe
