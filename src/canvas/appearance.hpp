#pragma once
#include "common/common.hpp"
#include "canvas/color_palette.hpp"
#include <map>

namespace horizon {
class Appearance {
public:
    Appearance();
    std::map<ColorP, Color> colors;
    std::map<int, Color> layer_colors;
    float grid_opacity = 1;
    float highlight_dim = .5;
    float highlight_shadow = .3;
    float highlight_lighten = .3;
    enum class GridStyle { CROSS, DOT, GRID };
    GridStyle grid_style = GridStyle::CROSS;
    unsigned int msaa = 4;
    enum class GridFineModifier { CTRL, ALT };
    GridFineModifier grid_fine_modifier = GridFineModifier::ALT;
};
} // namespace horizon
