#include "prj-mgr_views.hpp"
#include "pool-prj-mgr/pool-prj-mgr-app_win.hpp"
#include "pool-prj-mgr/pool-prj-mgr-app.hpp"
#include "project/project.hpp"
#include "pool/pool_manager.hpp"
#include "nlohmann/json.hpp"
#include "pool_cache_window.hpp"
#include "part_browser/part_browser_window.hpp"
#include "widgets/pool_chooser.hpp"

namespace horizon {

PoolProjectManagerViewCreateProject::PoolProjectManagerViewCreateProject(const Glib::RefPtr<Gtk::Builder> &builder,
                                                                         class PoolProjectManagerAppWindow *w)
    : win(w)
{
    builder->get_widget("create_project_name_entry", project_name_entry);
    builder->get_widget("create_project_description_entry", project_description_entry);
    builder->get_widget("create_project_path_chooser", project_path_chooser);
    builder->get_widget("create_project_dir_label", project_dir_label);
    builder->get_widget("create_project_pool_combo", project_pool_combo);

    project_name_entry->signal_changed().connect(sigc::mem_fun(this, &PoolProjectManagerViewCreateProject::update));
    project_description_entry->signal_changed().connect(
            sigc::mem_fun(this, &PoolProjectManagerViewCreateProject::update));
    project_pool_combo->signal_changed().connect(sigc::mem_fun(this, &PoolProjectManagerViewCreateProject::update));
    project_path_chooser->signal_file_set().connect(sigc::mem_fun(this, &PoolProjectManagerViewCreateProject::update));
}

void PoolProjectManagerViewCreateProject::clear()
{
    project_name_entry->set_text("");
    project_description_entry->set_text("");
    project_dir_label->set_text("");
    project_path_chooser->unselect_all();
}

void PoolProjectManagerViewCreateProject::populate_pool_combo(const Glib::RefPtr<Gtk::Application> &app)
{
    auto mapp = Glib::RefPtr<PoolProjectManagerApplication>::cast_dynamic(app);
    project_pool_combo->remove_all();
    for (const auto &it : PoolManager::get().get_pools()) {
        if (it.second.enabled)
            project_pool_combo->append((std::string)it.second.uuid, it.second.name);
    }
    project_pool_combo->set_active(0);
}

std::pair<bool, std::string> PoolProjectManagerViewCreateProject::create()
{
    bool r = false;
    std::string s;
    try {
        Project prj(UUID::random());
        prj.name = project_name_entry->get_text();
        prj.base_path = Glib::build_filename(project_path_chooser->get_file()->get_path(), prj.name);
        prj.title = project_description_entry->get_text();
        prj.pool_uuid = static_cast<std::string>(project_pool_combo->get_active_id());

        auto app = Glib::RefPtr<PoolProjectManagerApplication>::cast_dynamic(win->get_application());
        s = prj.create(PoolManager::get().get_by_uuid(prj.pool_uuid)->default_via);
        r = true;
    }
    catch (const std::exception &e) {
        r = false;
        auto top = dynamic_cast<Gtk::Window *>(project_name_entry->get_ancestor(GTK_TYPE_WINDOW));
        Gtk::MessageDialog md(*top, "Error creating project", false /* use_markup */, Gtk::MESSAGE_ERROR,
                              Gtk::BUTTONS_OK);
        md.set_secondary_text(e.what());
        md.run();
    }
    catch (const Glib::Error &e) {
        r = false;
        auto top = dynamic_cast<Gtk::Window *>(project_name_entry->get_ancestor(GTK_TYPE_WINDOW));
        Gtk::MessageDialog md(*top, "Error creating project", false /* use_markup */, Gtk::MESSAGE_ERROR,
                              Gtk::BUTTONS_OK);
        md.set_secondary_text(e.what());
        md.run();
    }
    return {r, s};
}

void PoolProjectManagerViewCreateProject::update()
{
    bool valid = true;
    if (project_name_entry->get_text().size() == 0)
        valid = false;
    if (project_description_entry->get_text().size() == 0)
        valid = false;
    if (project_path_chooser->get_filenames().size() == 0)
        valid = false;
    if (project_pool_combo->get_active_row_number() == -1)
        valid = false;
    if (valid) {
        auto base_path = project_path_chooser->get_file()->get_path();
        auto dir = Glib::build_filename(base_path, project_name_entry->get_text());
        project_dir_label->set_text(dir);
    }
    else {
        project_dir_label->set_text("");
    }
    signal_valid_change().emit(valid);
}

PoolProjectManagerViewProject::PoolProjectManagerViewProject(const Glib::RefPtr<Gtk::Builder> &builder,
                                                             PoolProjectManagerAppWindow *w)
    : win(w)
{
    builder->get_widget("button_top_schematic", button_top_schematic);
    builder->get_widget("button_board", button_board);
    builder->get_widget("button_part_browser", button_part_browser);
    builder->get_widget("button_pool_cache", button_pool_cache);
    builder->get_widget("entry_project_title", entry_project_title);
    builder->get_widget("label_pool_name", label_pool_name);
    builder->get_widget("label_pool_path", label_pool_path);
    builder->get_widget("label_project_directory", label_project_directory);
    builder->get_widget("prj_pool_change_button", button_change_pool);
    builder->get_widget("prj_pool_info_bar", pool_info_bar);

    pool_info_bar->hide();
    auto reopen_button = pool_info_bar->add_button("Reopen project", 1);
    reopen_button->signal_clicked().connect([this] {
        Glib::signal_idle().connect_once([this] {
            std::string prj_filename = win->project_filename;
            if (win->close_pool_or_project()) {
                win->open_file_view(Gio::File::create_for_path(prj_filename));
            }
        });
    });

    entry_project_title->signal_changed().connect([this] { win->project_needs_save = true; });

    button_top_schematic->signal_clicked().connect(
            sigc::mem_fun(this, &PoolProjectManagerViewProject::handle_button_top_schematic));
    button_board->signal_clicked().connect(sigc::mem_fun(this, &PoolProjectManagerViewProject::handle_button_board));
    button_part_browser->signal_clicked().connect(
            sigc::mem_fun(this, &PoolProjectManagerViewProject::handle_button_part_browser));
    button_pool_cache->signal_clicked().connect(
            sigc::mem_fun(this, &PoolProjectManagerViewProject::handle_button_pool_cache));
    button_change_pool->signal_clicked().connect(
            sigc::mem_fun(this, &PoolProjectManagerViewProject::handle_button_change_pool));
}


void PoolProjectManagerViewProject::handle_button_change_pool()
{
    PoolChooserDialog dia(win);
    dia.select_pool(win->project->pool_uuid);
    if (dia.run() == Gtk::RESPONSE_OK) {
        win->project->pool_uuid = dia.get_selected_pool();
        pool_info_bar->show();
        win->project_needs_save = true;
    }
}

void PoolProjectManagerViewProject::handle_button_top_schematic()
{
    auto prj = win->project.get();
    auto top_block = prj->get_top_block();
    std::vector<std::string> args = {top_block.schematic_filename, top_block.block_filename};
    win->spawn_for_project(PoolProjectManagerProcess::Type::IMP_SCHEMATIC, args);
}

void PoolProjectManagerViewProject::handle_button_board()
{
    auto prj = win->project.get();
    auto top_block =
            std::find_if(prj->blocks.begin(), prj->blocks.end(), [](const auto &a) { return a.second.is_top; });
    if (top_block != prj->blocks.end()) {
        std::vector<std::string> args = {prj->board_filename, top_block->second.block_filename, prj->vias_directory};
        win->spawn_for_project(PoolProjectManagerProcess::Type::IMP_BOARD, args);
    }
}

void PoolProjectManagerViewProject::handle_button_part_browser()
{
    win->part_browser_window->present();
}

void PoolProjectManagerViewProject::handle_button_pool_cache()
{
    win->pool_cache_window->refresh_list();
    win->pool_cache_window->present();
}

} // namespace horizon
