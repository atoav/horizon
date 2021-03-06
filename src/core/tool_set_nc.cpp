#include "tool_set_nc.hpp"
#include "core_schematic.hpp"
#include "imp/imp_interface.hpp"
#include <iostream>

namespace horizon {

ToolSetNotConnected::ToolSetNotConnected(Core *c, ToolID tid) : ToolBase(c, tid)
{
}

bool ToolSetNotConnected::can_begin()
{
    return core.c;
}

ToolResponse ToolSetNotConnected::begin(const ToolArgs &args)
{
    update_tip();
    return ToolResponse();
}

void ToolSetNotConnected::update_tip()
{
    std::string s = "<b>LMB:</b>set pin NC <b>RMB:</b>cancel <b>m:</b>mode <i>";
    switch (mode) {
    case Mode::SET:
        s += "Set";
        break;
    case Mode::CLEAR:
        s += "Clear";
        break;
    case Mode::TOGGLE:
        s += "Toggle";
        break;
    }
    s += "</i>";

    imp->tool_bar_set_tip(s);
}


ToolResponse ToolSetNotConnected::update(const ToolArgs &args)
{
    if (args.type == ToolEventType::CLICK) {
        if (args.button == 1) {
            if (args.target.type == ObjectType::SYMBOL_PIN) {
                auto sym = core.c->get_schematic_symbol(args.target.path.at(0));
                auto pin = &sym->symbol.pins.at(args.target.path.at(1));
                UUIDPath<2> connpath(sym->gate->uuid, args.target.path.at(1));
                if (sym->component->connections.count(connpath) == 0) { // pin is not connected
                    if (mode == Mode::TOGGLE || mode == Mode::SET) {
                        sym->component->connections.emplace(connpath, nullptr);
                        sym->symbol.pins.at(pin->uuid).connector_style = SymbolPin::ConnectorStyle::NC; // for preview
                    }
                }
                else {
                    auto &conn = sym->component->connections.at(connpath);
                    if (conn.net == nullptr) {
                        if (mode == Mode::TOGGLE || mode == Mode::CLEAR) {
                            sym->component->connections.erase(connpath);
                            sym->symbol.pins.at(pin->uuid).connector_style =
                                    SymbolPin::ConnectorStyle::BOX; // for preview
                        }
                    }
                    else {
                        imp->tool_bar_flash("Pin is connected to net");
                    }
                }
            }
            else {
                imp->tool_bar_flash("Please click on a pin");
            }
        }
        else if (args.button == 3) {
            core.r->commit();
            return ToolResponse::end();
        }
    }
    else if (args.type == ToolEventType::KEY) {
        if (args.key == GDK_KEY_m) {
            switch (mode) {
            case Mode::SET:
                mode = Mode::CLEAR;
                break;

            case Mode::CLEAR:
                mode = Mode::TOGGLE;
                break;

            case Mode::TOGGLE:
                mode = Mode::SET;
                break;
            }
            update_tip();
        }
    }

    return ToolResponse();
}
} // namespace horizon
