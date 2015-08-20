#pragma once

#include "solar/rendering/textures/uv.h"
#include "solar/rendering/meshes/mesh_def.h"
#include "solar/utility/optional.h"
#include <fbxsdk.h>
#include <memory>
#include <unordered_map>
#include "fbx_converter_mesh_data.h"
#include "fbx_polygon_vertex_data.h"

namespace solar {

	class fbx_converter {
	private:
		bool _is_verbose;
		bool _is_warnings_as_errors_enabled;

		std::shared_ptr<mesh_def> _mesh_def;
		fbx_converter_mesh_data _mesh_data;
		std::vector<fbx_polygon_vertex_data> _unduped_vertices;

	public:
		fbx_converter();
		std::shared_ptr<mesh_def> convert_fbx_to_mesh_def(std::string path);

	private:
		void reset_internals();
		void find_and_process_fbx_mesh(FbxScene* scene);
		void find_fbx_meshes_recursive(std::vector<FbxMesh*>& mesh_nodes, FbxNode* node);
		void process_fbx_mesh(FbxMesh* mesh);
		void build_mesh_data(FbxMesh* mesh);
		fbx_converter_mesh_data::material make_mesh_data_material(FbxSurfaceMaterial* in_material);
		std::string get_texture_file_name(FbxFileTexture* fbx_file_texture, const char* texture_type);
		void handle_missing_mesh_data();
		void build_unduped_vertices();
		void sort_polygons_by_material_index();
		void build_mesh_def();

		template<typename ElementT>
		void process_mesh_element_per_polygon_with_index_to_direct(
			FbxMesh* mesh,
			const char* element_name,
			std::function<int(FbxMesh*)> get_element_count,
			std::function<ElementT*(FbxMesh*, int)> get_element,
			std::function<void(fbx_converter_mesh_data::polygon* polygon, int index_to_direct)> process_polygon);

		template<typename ElementT, typename ValueT> 
		void process_mesh_element_per_polygon_vertex(
			FbxMesh* mesh,
			const char* element_name,
			std::function<int(FbxMesh*)> get_element_count,
			std::function<ElementT*(FbxMesh*, int)> get_element,
			std::function<void(fbx_converter_mesh_data::polygon_vertex* vertex, ValueT value)> process_vertex);

		void add_verbose_message(const std::string& message);
		void add_warning_message(const std::string& message);
		void add_error_message(const std::string& message);

	private:
		static vec3 convert_fbx_to_vec3(const FbxVector4& v);
		static uv convert_fbx_to_uv(const FbxVector2& v);
	};


	template<typename ElementT>
	void fbx_converter::process_mesh_element_per_polygon_with_index_to_direct(
		FbxMesh* mesh,
		const char* element_name,
		std::function<int(FbxMesh*)> get_element_count,
		std::function<ElementT*(FbxMesh*, int)> get_element,
		std::function<void(fbx_converter_mesh_data::polygon* polygon, int index_to_direct)> process_polygon) {

		for (int i_element = 0; i_element < get_element_count(mesh); ++i_element) {
			auto element = get_element(mesh, i_element);

			auto mapping_mode = element->GetMappingMode();
			auto reference_mode = element->GetReferenceMode();

			if (mapping_mode == FbxLayerElement::eByPolygon && reference_mode == FbxLayerElement::eIndexToDirect) {
				for (int i_polygon = 0; i_polygon < element->GetIndexArray().GetCount(); ++i_polygon) {
					int direct_index = element->GetIndexArray().GetAt(i_polygon);
					process_polygon(_mesh_data._polygons.at(i_polygon).get(), direct_index);
				}
			}
			else if (mapping_mode == FbxLayerElement::eAllSame && reference_mode == FbxLayerElement::eIndexToDirect) {
				if (element->GetIndexArray().GetCount() != 1) {
					add_error_message(build_string("eAllSame IndexArrayCount expected to be one in {}", element_name));
				}
				else {
					int direct_index = element->GetIndexArray().GetAt(0);
					for (auto polygon : _mesh_data._polygons) {
						process_polygon(polygon.get(), direct_index);
					}
				}
			}
			else {
				add_error_message(build_string("Unhandled reference_mode and mapping_mode combination in {} : {} - {}", element_name, fbx_reference_mode_to_string(reference_mode), fbx_mapping_mode_to_string(mapping_mode)));
			}
		}
	}

	template<typename ElementT, typename ValueT>
	void fbx_converter::process_mesh_element_per_polygon_vertex(
		FbxMesh* mesh,
		const char* element_name,
		std::function<int(FbxMesh*)> get_element_count,
		std::function<ElementT*(FbxMesh*, int)> get_element,
		std::function<void(fbx_converter_mesh_data::polygon_vertex* vertex, ValueT value)> process_vertex) {

		for (int i_element = 0; i_element < get_element_count(mesh); ++i_element) {
			auto element = get_element(mesh, i_element);

			auto mapping_mode = element->GetMappingMode();
			auto reference_mode = element->GetReferenceMode();

			if (mapping_mode == FbxLayerElement::eByControlPoint && reference_mode == FbxLayerElement::eDirect) {
				for (int i_control_point = 0; i_control_point < element->GetDirectArray().GetCount(); ++i_control_point) {
					auto cp = _mesh_data._control_points.at(i_control_point);
					for (auto pv : cp->_vertices) {
						process_vertex(pv.get(), element->GetDirectArray().GetAt(i_control_point));
					}
				}
			}
			else if (mapping_mode == FbxLayerElement::eByPolygonVertex && reference_mode == FbxLayerElement::eDirect) {
				for (int i_polygon_vertex = 0; i_polygon_vertex < element->GetDirectArray().GetCount(); ++i_polygon_vertex) {
					auto pv = _mesh_data._polygon_vertices.at(i_polygon_vertex);
					process_vertex(pv.get(), element->GetDirectArray().GetAt(i_polygon_vertex));
				}
			}
			else if (mapping_mode == FbxLayerElement::eByPolygonVertex && reference_mode == FbxLayerElement::eIndexToDirect) {
				for (int i_polygon_vertex = 0; i_polygon_vertex < element->GetIndexArray().GetCount(); ++i_polygon_vertex) {
					int direct_index = element->GetIndexArray().GetAt(i_polygon_vertex);
					auto pv = _mesh_data._polygon_vertices.at(i_polygon_vertex);
					process_vertex(pv.get(), element->GetDirectArray().GetAt(direct_index));
				}
			}
			else {
				add_error_message(build_string("Unhandled reference_mode and mapping_mode combination in {} : {} - {}", element_name, fbx_reference_mode_to_string(reference_mode), fbx_mapping_mode_to_string(mapping_mode)));
			}

		}
	}


}