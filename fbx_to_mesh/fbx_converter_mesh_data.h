#pragma once

#include <array>
#include <vector>
#include <memory>
#include "fbx_polygon_vertex_data.h"
#include "solar/utility/optional.h"
#include "solar/strings/string_build.h"

namespace solar {

	class fbx_converter_mesh_data {
	public:
		class polygon_vertex {
		public:
			fbx_polygon_vertex_data _data;
			optional<int> _unduped_vertex_index; //index into unduped_vertices
		};

		class polygon {
		public:
			std::array<std::shared_ptr<polygon_vertex>, 3> _vertices;
			optional<unsigned int> _material_index;
		};

		class control_point {
		public:
			std::vector<std::shared_ptr<polygon_vertex>> _vertices;
		};

		class material {
		public:
			std::string _diffuse_map_file_name;
			std::string _normal_map_file_name;

		public:
			std::string to_string() const {
				return build_string("{{ diffuse_map:'{}' , normal_map:'{}' }}", _diffuse_map_file_name, _normal_map_file_name);
			}
		};

	public:
		std::vector<material> _materials;
		std::vector<std::shared_ptr<polygon_vertex>> _polygon_vertices;
		std::vector<std::shared_ptr<polygon>> _polygons;
		std::vector<std::shared_ptr<control_point>> _control_points;
	};

}