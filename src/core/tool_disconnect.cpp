#include "tool_disconnect.hpp"
#include "core_schematic.hpp"
#include "core_board.hpp"
#include <iostream>

namespace horizon {

ToolDisconnect::ToolDisconnect(Core *c, ToolID tid) : ToolBase(c, tid)
{
}

bool ToolDisconnect::can_begin()
{
    for (const auto &it : core.r->selection) {
        if (it.type == ObjectType::SCHEMATIC_SYMBOL) {
            return true;
        }
        else if (it.type == ObjectType::BOARD_PACKAGE) {
            return true;
        }
    }
    return false;
}

ToolResponse ToolDisconnect::begin(const ToolArgs &args)
{
    std::cout << "tool disconnect\n";
    for (const auto &it : args.selection) {
        if (it.type == ObjectType::SCHEMATIC_SYMBOL) {
            core.c->get_schematic()->disconnect_symbol(core.c->get_sheet(), core.c->get_schematic_symbol(it.uuid));
        }
        else if (it.type == ObjectType::BOARD_PACKAGE) {
            core.b->get_board()->disconnect_package(&core.b->get_board()->packages.at(it.uuid));
        }
    }
    core.r->commit();
    return ToolResponse::end();
}
ToolResponse ToolDisconnect::update(const ToolArgs &args)
{
    return ToolResponse();
}
} // namespace horizon
