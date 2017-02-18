#pragma once
#include "imp.hpp"

namespace horizon {
	class ImpSchematic : public ImpBase {
		public :
			ImpSchematic(const std::string &schematic_filename, const std::string &block_filename, const std::string &constraints_filename, const std::string &pool_path);


		protected:
			void construct() override;
			ToolID handle_key(guint k) override;
		private:
			void canvas_update() override;
			CoreSchematic core_schematic;
			int handle_ask_net_merge(class Net *net, class Net *into);
			int handle_ask_delete_component(class Component *comp);
			void handle_select_sheet(Sheet *sh);
			void handle_remove_sheet(Sheet *sh);
			void handle_core_rebuilt();
			void handle_tool_change(ToolID id) override;
			void handle_export_pdf();
			std::string last_pdf_filename;

			std::map<UUID, std::pair<float, Coordf>> sheet_views;
			SheetBox *sheet_box;
	};
}