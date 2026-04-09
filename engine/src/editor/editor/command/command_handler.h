//
// Created by GreenMuffin on 2025/12/9.
//

#ifndef CAKERY_COMMAND_HANDLER_H
#define CAKERY_COMMAND_HANDLER_H
#include "command.h"
#include "dopch.h"

namespace cakery {
    class CommandHandler {
    public:
        CommandHandler();
        ~CommandHandler();

        void execute();
        static void push_command(const dodoe::Ref<Command>& command);
        static void undo();
        static void redo();

    private:
        static std::vector<dodoe::Ref<Command>> execute_command_this_frame_;
        static std::vector<dodoe::Ref<Command>> undo_command_this_frame_;
        static std::vector<dodoe::Ref<Command>> redo_command_this_frame_;
        static std::vector<dodoe::Ref<Command>> command_history_;
        static std::vector<dodoe::Ref<Command>> redo_stack_;
    };
} // cakery


#endif //CAKERY_COMMAND_HANDLER_H
