#include "package.hpp"
#include "clipper/clipper.hpp"
#include "pool.hpp"
#include "util/util.hpp"
#include "nlohmann/json.hpp"

namespace horizon {

Package::MyParameterProgram::MyParameterProgram(Package *p, const std::string &c) : ParameterProgramPolygon(c), pkg(p)
{
}

std::map<UUID, Polygon> &Package::MyParameterProgram::get_polygons()
{
    return pkg->polygons;
}

ParameterProgram::CommandHandler Package::MyParameterProgram::get_command(const std::string &cmd)
{
    using namespace std::placeholders;
    if (auto r = ParameterProgram::get_command(cmd)) {
        return r;
    }
    else if (cmd == "set-polygon") {
        return std::bind(std::mem_fn(&Package::MyParameterProgram::set_polygon), this, _1, _2);
    }
    else if (cmd == "set-polygon-vertices") {
        return std::bind(std::mem_fn(&Package::MyParameterProgram::set_polygon_vertices), this, _1, _2);
    }
    else if (cmd == "expand-polygon") {
        return std::bind(std::mem_fn(&Package::MyParameterProgram::expand_polygon), this, _1, _2);
    }
    return nullptr;
}

Package::Model::Model(const UUID &uu, const std::string &fn) : uuid(uu), filename(fn)
{
}
Package::Model::Model(const UUID &uu, const json &j)
    : uuid(uu), filename(j.at("filename").get<std::string>()), x(j.at("x")), y(j.at("y")), z(j.at("z")),
      roll(j.at("roll")), pitch(j.at("pitch")), yaw(j.at("yaw"))
{
}

json Package::Model::serialize() const
{
    json j;
    j["filename"] = filename;
    j["x"] = x;
    j["y"] = y;
    j["z"] = z;
    j["roll"] = roll;
    j["pitch"] = pitch;
    j["yaw"] = yaw;

    return j;
}

Package::Package(const UUID &uu, const json &j, Pool &pool)
    : uuid(uu), name(j.at("name").get<std::string>()), manufacturer(j.value("manufacturer", "")),
      parameter_program(this, j.value("parameter_program", ""))
{
    {
        const json &o = j["junctions"];
        for (auto it = o.cbegin(); it != o.cend(); ++it) {
            auto u = UUID(it.key());
            junctions.emplace(std::make_pair(u, Junction(u, it.value())));
        }
    }
    {
        const json &o = j["lines"];
        for (auto it = o.cbegin(); it != o.cend(); ++it) {
            auto u = UUID(it.key());
            lines.emplace(std::make_pair(u, Line(u, it.value(), *this)));
        }
    }
    {
        const json &o = j["arcs"];
        for (auto it = o.cbegin(); it != o.cend(); ++it) {
            auto u = UUID(it.key());
            arcs.emplace(std::make_pair(u, Arc(u, it.value(), *this)));
        }
    }
    {
        const json &o = j["texts"];
        for (auto it = o.cbegin(); it != o.cend(); ++it) {
            auto u = UUID(it.key());
            texts.emplace(std::make_pair(u, Text(u, it.value())));
        }
    }
    {
        const json &o = j["pads"];
        for (auto it = o.cbegin(); it != o.cend(); ++it) {
            auto u = UUID(it.key());
            pads.emplace(std::make_pair(u, Pad(u, it.value(), pool)));
        }
    }

    if (j.count("polygons")) {
        const json &o = j["polygons"];
        for (auto it = o.cbegin(); it != o.cend(); ++it) {
            auto u = UUID(it.key());
            polygons.emplace(std::make_pair(u, Polygon(u, it.value())));
        }
    }
    if (j.count("keepouts")) {
        const json &o = j["keepouts"];
        for (auto it = o.cbegin(); it != o.cend(); ++it) {
            auto u = UUID(it.key());
            keepouts.emplace(std::piecewise_construct, std::forward_as_tuple(u),
                             std::forward_as_tuple(u, it.value(), *this));
        }
    }
    if (j.count("dimensions")) {
        const json &o = j["dimensions"];
        for (auto it = o.cbegin(); it != o.cend(); ++it) {
            auto u = UUID(it.key());
            dimensions.emplace(std::piecewise_construct, std::forward_as_tuple(u),
                               std::forward_as_tuple(u, it.value()));
        }
    }
    for (auto &it : keepouts) {
        it.second.polygon.update(polygons);
        it.second.polygon->usage = &it.second;
    }
    map_erase_if(polygons, [](const auto &a) { return a.second.vertices.size() == 0; });
    if (j.count("tags")) {
        tags = j.at("tags").get<std::set<std::string>>();
    }
    if (j.count("parameter_set")) {
        parameter_set = parameter_set_from_json(j.at("parameter_set"));
    }
    if (j.count("alternate_for")) {
        alternate_for = pool.get_package(UUID(j.at("alternate_for").get<std::string>()));
    }
    if (j.count("model_filename")) {
        std::string mfn = j.at("model_filename");
        if (mfn.size()) {
            auto m_uu = UUID::random();
            models.emplace(std::piecewise_construct, std::forward_as_tuple(m_uu),
                           std::forward_as_tuple(m_uu, j.at("model_filename").get<std::string>()));
            default_model = m_uu;
        }
    }
    if (j.count("models")) {
        const json &o = j["models"];
        for (auto it = o.cbegin(); it != o.cend(); ++it) {
            auto u = UUID(it.key());
            models.emplace(std::piecewise_construct, std::forward_as_tuple(u), std::forward_as_tuple(u, it.value()));
        }
        default_model = j.at("default_model").get<std::string>();
    }
}

Package Package::new_from_file(const std::string &filename, Pool &pool)
{
    json j;
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        throw std::runtime_error("file " + filename + " not opened");
    }
    ifs >> j;
    ifs.close();
    return Package(UUID(j["uuid"].get<std::string>()), j, pool);
}

Package::Package(const UUID &uu) : uuid(uu), parameter_program(this, "")
{
}

Junction *Package::get_junction(const UUID &uu)
{
    return &junctions.at(uu);
}

Polygon *Package::get_polygon(const UUID &uu)
{
    return &polygons.at(uu);
}

Package::Package(const Package &pkg)
    : uuid(pkg.uuid), name(pkg.name), manufacturer(pkg.manufacturer), tags(pkg.tags), junctions(pkg.junctions),
      lines(pkg.lines), arcs(pkg.arcs), texts(pkg.texts), pads(pkg.pads), polygons(pkg.polygons),
      keepouts(pkg.keepouts), dimensions(pkg.dimensions), parameter_set(pkg.parameter_set),
      parameter_program(pkg.parameter_program), models(pkg.models), default_model(pkg.default_model),
      alternate_for(pkg.alternate_for), warnings(pkg.warnings)
{
    update_refs();
}

void Package::operator=(Package const &pkg)
{
    uuid = pkg.uuid;
    name = pkg.name;
    manufacturer = pkg.manufacturer;
    tags = pkg.tags;
    junctions = pkg.junctions;
    lines = pkg.lines;
    arcs = pkg.arcs;
    texts = pkg.texts;
    pads = pkg.pads;
    polygons = pkg.polygons;
    keepouts = pkg.keepouts;
    dimensions = pkg.dimensions;
    parameter_set = pkg.parameter_set;
    parameter_program = pkg.parameter_program;
    models = pkg.models;
    default_model = pkg.default_model;
    alternate_for = pkg.alternate_for;
    warnings = pkg.warnings;
    update_refs();
}

void Package::update_refs(Pool &pool)
{
    for (auto &it : pads) {
        it.second.pool_padstack = pool.get_padstack(it.second.pool_padstack.uuid);
        it.second.padstack = *it.second.pool_padstack;
    }
    update_refs();
}

void Package::update_refs()
{
    for (auto &it : lines) {
        auto &line = it.second;
        line.to = &junctions.at(line.to.uuid);
        line.from = &junctions.at(line.from.uuid);
    }
    for (auto &it : arcs) {
        auto &arc = it.second;
        arc.to = &junctions.at(arc.to.uuid);
        arc.from = &junctions.at(arc.from.uuid);
        arc.center = &junctions.at(arc.center.uuid);
    }
    for (auto &it : keepouts) {
        it.second.polygon.update(polygons);
        it.second.polygon->usage = &it.second;
    }
    parameter_program.pkg = this;
}

static void copy_param(ParameterSet &dest, const ParameterSet &src, ParameterID id)
{
    if (src.count(id))
        dest[id] = src.at(id);
}

static void copy_param(ParameterSet &dest, const ParameterSet &src, const std::set<ParameterID> &ids)
{
    for (const auto id : ids) {
        copy_param(dest, src, id);
    }
}

std::pair<bool, std::string> Package::apply_parameter_set(const ParameterSet &ps)
{
    auto ps_this = parameter_set;
    copy_param(ps_this, ps, ParameterID::COURTYARD_EXPANSION);
    {
        auto r = parameter_program.run(ps_this);
        if (r.first) {
            return r;
        }
    }

    for (auto &it : pads) {
        auto ps_pad = it.second.parameter_set;
        copy_param(ps_pad, ps,
                   {ParameterID::SOLDER_MASK_EXPANSION, ParameterID::PASTE_MASK_CONTRACTION,
                    ParameterID::HOLE_SOLDER_MASK_EXPANSION});
        auto r = it.second.padstack.apply_parameter_set(ps_pad);
        if (r.first) {
            return {r.first, "Pad " + it.second.name + ": " + r.second};
        }
    }
    return {false, ""};
}

void Package::expand()
{
    map_erase_if(keepouts, [this](auto &it) { return polygons.count(it.second.polygon.uuid) == 0; });
    for (auto &it : junctions) {
        it.second.temp = false;
        it.second.layer = 10000;
        it.second.has_via = false;
        it.second.needs_via = false;
        it.second.connection_count = 0;
    }

    for (const auto &it : lines) {
        it.second.from->connection_count++;
        it.second.to->connection_count++;
        for (auto &ju : {it.second.from, it.second.to}) {
            if (ju->layer == 10000) { // none assigned
                ju->layer = it.second.layer;
            }
        }
    }
    for (const auto &it : arcs) {
        it.second.from->connection_count++;
        it.second.to->connection_count++;
        for (auto &ju : {it.second.from, it.second.to}) {
            if (ju->layer == 10000) { // none assigned
                ju->layer = it.second.layer;
            }
        }
    }
}

void Package::update_warnings()
{
    warnings.clear();
    std::set<std::string> pad_names;
    for (const auto &it : pads) {
        auto x = pad_names.insert(it.second.name);
        if (!x.second) {
            warnings.emplace_back(it.second.placement.shift, "duplicate pad name");
        }
        for (auto p : it.second.pool_padstack->parameters_required) {
            if (it.second.parameter_set.count(p) == 0) {
                warnings.emplace_back(it.second.placement.shift, "missing parameter " + parameter_id_to_name(p));
            }
        }
    }
}

std::pair<Coordi, Coordi> Package::get_bbox() const
{
    Coordi a;
    Coordi b;
    for (const auto &it : pads) {
        auto bb_pad = it.second.placement.transform_bb(it.second.padstack.get_bbox());
        a = Coordi::min(a, bb_pad.first);
        b = Coordi::max(b, bb_pad.second);
    }
    return std::make_pair(a, b);
}

const std::map<int, Layer> &Package::get_layers() const
{
    static const std::map<int, Layer> layers = {{60, {60, "Top Courtyard"}},
                                                {50, {50, "Top Assembly"}},
                                                {40, {40, "Top Package"}},
                                                {30, {30, "Top Paste"}},
                                                {20, {20, "Top Silkscreen"}},
                                                {10, {10, "Top Mask"}},
                                                {0, {0, "Top Copper", false, true}},
                                                {-1, {-1, "Inner", false, true}},
                                                {-100, {-100, "Bottom Copper", true, true}},
                                                {-110, {-110, "Bottom Mask", true}},
                                                {-120, {-120, "Bottom Silkscreen", true}},
                                                {-130, {-130, "Bottom Paste"}},
                                                {-140, {-140, "Bottom Package"}},
                                                {-150, {-150, "Bottom Assembly", true}},
                                                {-160, {-160, "Bottom Courtyard"}}};
    return layers;
}

json Package::serialize() const
{
    json j;
    j["uuid"] = (std::string)uuid;
    j["type"] = "package";
    j["name"] = name;
    j["manufacturer"] = manufacturer;
    j["tags"] = tags;
    j["parameter_program"] = parameter_program.get_code();
    j["parameter_set"] = parameter_set_serialize(parameter_set);
    if (alternate_for && alternate_for->uuid != uuid)
        j["alternate_for"] = (std::string)alternate_for->uuid;
    j["models"] = json::object();
    for (const auto &it : models) {
        j["models"][(std::string)it.first] = it.second.serialize();
    }
    j["default_model"] = (std::string)default_model;
    j["junctions"] = json::object();
    for (const auto &it : junctions) {
        j["junctions"][(std::string)it.first] = it.second.serialize();
    }
    j["lines"] = json::object();
    for (const auto &it : lines) {
        j["lines"][(std::string)it.first] = it.second.serialize();
    }
    j["arcs"] = json::object();
    for (const auto &it : arcs) {
        j["arcs"][(std::string)it.first] = it.second.serialize();
    }
    j["texts"] = json::object();
    for (const auto &it : texts) {
        j["texts"][(std::string)it.first] = it.second.serialize();
    }
    j["pads"] = json::object();
    for (const auto &it : pads) {
        j["pads"][(std::string)it.first] = it.second.serialize();
    }
    j["polygons"] = json::object();
    for (const auto &it : polygons) {
        j["polygons"][(std::string)it.first] = it.second.serialize();
    }
    j["keepouts"] = json::object();
    for (const auto &it : keepouts) {
        j["keepouts"][(std::string)it.first] = it.second.serialize();
    }
    j["dimensions"] = json::object();
    for (const auto &it : dimensions) {
        j["dimensions"][(std::string)it.first] = it.second.serialize();
    }
    return j;
}

UUID Package::get_uuid() const
{
    return uuid;
}

const Package::Model *Package::get_model(const UUID &uu) const
{
    UUID uu2 = uu;
    if (uu2 == UUID()) {
        uu2 = default_model;
    }
    if (models.count(uu2)) {
        return &models.at(uu2);
    }
    else {
        return nullptr;
    }
}

int Package::get_max_pad_name() const
{
    std::vector<int> pad_nrs;
    for (const auto &it : pads) {
        try {
            int n = std::stoi(it.second.name);
            pad_nrs.push_back(n);
        }
        catch (...) {
        }
    }
    if (pad_nrs.size()) {
        int maxpad = *std::max_element(pad_nrs.begin(), pad_nrs.end());
        return maxpad;
    }
    return -1;
}
} // namespace horizon
