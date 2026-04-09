//
// Created by GreenMuffin on 2025/12/12.
//

#ifndef CAKERY_PROPERTIES_PANEL_CMD_H
#define CAKERY_PROPERTIES_PANEL_CMD_H


namespace cakery {
    class PropertiesPanelHandler;

    class PropertiesPanelCmd {
    public:
        virtual ~PropertiesPanelCmd() = default;
        virtual void execute(PropertiesPanelHandler& handler) = 0;
        virtual void undo(PropertiesPanelHandler& handler) = 0;
    };
} // cakery


#endif //CAKERY_PROPERTIES_PANEL_CMD_H