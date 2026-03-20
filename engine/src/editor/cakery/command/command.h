//
// Created by GreenMuffin on 2025/12/9.
//

#ifndef CAKERY_COMMAND_H
#define CAKERY_COMMAND_H

namespace cakery {
    class Command{
    public:
        virtual ~Command() = default;

        virtual void execute() = 0;
        virtual void undo() = 0;
        virtual void redo() { execute(); }
    };
} // cakery

#endif //CAKERY_COMMAND_H