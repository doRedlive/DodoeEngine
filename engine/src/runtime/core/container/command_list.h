// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/memory/allocator.h"

namespace dodoe {

    template <typename TExecutor>
    class CommandList {
    public:
        inline static constexpr Size_t kDefaultBlockSize = 4096;

        struct Command {
            Command* m_next{nullptr};
            Size_t m_size{0};
            void (*m_execute)(const Command&, TExecutor&);
            void (*m_destroy)(Command&);

            Command(Size_t size, void (*execute)(const Command&, TExecutor&), void (*destroy)(Command&))
                : m_size(size), m_execute(execute), m_destroy(destroy) {}
        };

        template <typename TDerived>
        struct CommandImpl : Command {
            CommandImpl() : Command(sizeof(TDerived), &ExecuteCommand, &DestroyCommand) {}

        private:
            static void ExecuteCommand(const Command& cmd, TExecutor& executor) {
                static_cast<const TDerived&>(cmd).execute(executor);
            }

            static void DestroyCommand(Command& cmd) {
                static_cast<TDerived&>(cmd).~TDerived();
            }
        };

        CommandList() = default;
        ~CommandList() { reset(); }

        CommandList(const CommandList&) = delete;
        CommandList& operator=(const CommandList&) = delete;

        CommandList(CommandList&& other) noexcept { moveFrom(std::move(other)); }
        CommandList& operator=(CommandList&& other) noexcept {
            if (this != &other) {
                reset();
                moveFrom(std::move(other));
            }
            return *this;
        }

        template <typename TCommand, typename... TArgs>
        TCommand& enqueue(TArgs&&... args) {
            static_assert(std::is_base_of_v<Command, TCommand>, "TCommand must derive from CommandList::Command");
            static_assert(alignof(TCommand) <= alignof(std::max_align_t));

            void* memory = allocate(sizeof(TCommand), alignof(TCommand));
            auto* command = new (memory) TCommand(std::forward<TArgs>(args)...);
            appendCommand(command);
            return *command;
        }

        void beginImmediateFrame(TExecutor& target) { m_immediate_target = &target; }
        void endImmediateFrame() { m_immediate_target = nullptr; }
        [[nodiscard]] bool isImmediate() const { return m_immediate_target != nullptr; }

        void append(CommandList&& other) {
            if (other.isEmpty()) return;

            if (m_tail != nullptr) {
                m_tail->m_next = other.m_head;
            }
            else {
                m_head = other.m_head;
            }

            m_tail = other.m_tail;
            m_command_count += other.m_command_count;

            other.m_head = nullptr;
            other.m_tail = nullptr;
            other.m_command_count = 0;
        }

        void execute(TExecutor& executor) const {
            const Command* cmd = m_head;
            while (cmd != nullptr) {
                DO_ASSERT(cmd->m_execute != nullptr, "CommandList execute function is null");
                cmd->m_execute(*cmd, executor);
                cmd = cmd->m_next;
            }
        }

        void reset() {
            Command* cmd = m_head;
            while (cmd != nullptr) {
                Command* next = cmd->m_next;
                DO_ASSERT(cmd->m_destroy != nullptr, "CommandList destroy function is null");
                cmd->m_destroy(*cmd);
                cmd = next;
            }
            m_head = nullptr;
            m_tail = nullptr;
            m_command_count = 0;
            Memory::AdvanceFrameEpoch();
        }

        [[nodiscard]] bool isEmpty() const { return m_head == nullptr; }
        [[nodiscard]] Size_t commandCount() const { return m_command_count; }

    protected:
        void appendCommand(Command* command) {
            DO_ASSERT(command != nullptr, "CommandList command is null");

            command->m_next = nullptr;
            if (m_tail != nullptr) {
                m_tail->m_next = command;
            }
            else {
                m_head = command;
            }
            m_tail = command;
            ++m_command_count;
        }

        [[nodiscard]] void* allocate(Size_t size, Size_t alignment) {
            return Memory::AllocateFrame(size, alignment, AllocTag::RenderCmd);
        }

        void moveFrom(CommandList&& other) {
            m_head = other.m_head;
            m_tail = other.m_tail;
            m_command_count = other.m_command_count;
            m_immediate_target = other.m_immediate_target;

            other.m_head = nullptr;
            other.m_tail = nullptr;
            other.m_command_count = 0;
        }

        [[nodiscard]] TExecutor* immediateTarget() const { return m_immediate_target; }

    private:
        Command* m_head{nullptr};
        Command* m_tail{nullptr};
        Size_t m_command_count{0};
        TExecutor* m_immediate_target{nullptr};
    };

} // namespace dodoe
