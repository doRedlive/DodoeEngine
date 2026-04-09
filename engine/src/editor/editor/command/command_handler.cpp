//
// Created by GreenMuffin on 2025/12/9.
//

#include "command_handler.h"

#include "runtime/function/context.h"

namespace cakery {
    std::vector<dodoe::Ref<Command>> CommandHandler::execute_command_this_frame_{};
    std::vector<dodoe::Ref<Command>> CommandHandler::undo_command_this_frame_{};
    std::vector<dodoe::Ref<Command>> CommandHandler::redo_command_this_frame_{};
    std::vector<dodoe::Ref<Command>> CommandHandler::command_history_{};
    std::vector<dodoe::Ref<Command>> CommandHandler::redo_stack_{};

    CommandHandler::CommandHandler() {
        dodoe::g_context.event_system->subscribe_event<dodoe::AfterOneTickEvent, &CommandHandler::execute>(this);
    }

    CommandHandler::~CommandHandler() {
        dodoe::g_context.event_system->unsubscribe_event<dodoe::AfterOneTickEvent, &CommandHandler::execute>(this);
        execute_command_this_frame_.clear();
        command_history_.clear();
        redo_stack_.clear();
    }

    void CommandHandler::execute() {
        for (const auto& command : execute_command_this_frame_) {
            command->execute();
        }
        for (const auto& command : undo_command_this_frame_) {
            command->undo();
        }
        for (const auto& command : redo_command_this_frame_) {
            command->redo();
        }
        execute_command_this_frame_.clear();
        undo_command_this_frame_.clear();
        redo_command_this_frame_.clear();
    }

    void CommandHandler::push_command(const dodoe::Ref<Command> &command) {
        execute_command_this_frame_.push_back(command);
        command_history_.push_back(command);
        redo_stack_.clear();
    }

    void CommandHandler::undo() {
        if (command_history_.empty()) return;
        DoDebug("undo");
        const auto command = command_history_.back();
        undo_command_this_frame_.push_back(command);
        command_history_.pop_back();
        redo_stack_.push_back(command);
    }

    void CommandHandler::redo() {
        if (redo_stack_.empty()) return;
        const auto command = redo_stack_.back();
        DoDebug("redo");
        redo_command_this_frame_.push_back(command);
        redo_stack_.pop_back();
        command_history_.push_back(command);
    }
}
