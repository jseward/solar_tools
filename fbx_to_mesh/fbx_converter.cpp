#include "fbx_converter.h"

#include "solar/utility/assert.h"
#include "solar/utility/alert.h"
#include "solar/utility/trace.h"
#include "solar/utility/type_convert.h"
#include "solar/strings/string_build.h"
#include "solar/math/mat33.h"
#include "solar/io/file_path_helpers.h"
#include "fbx_enum_helpers.h"

//useful references
//---
//- http://www.gamedev.net/page/resources/_/technical/graphics-programming-and-theory/how-to-work-with-fbx-sdk-r3582
//- FBX SDK/samples/ExportScene03
//- FBX SDK/ViewScene/SceneContext.cxx

namespace solar {

	fbx_converter::fbx_converter() 
		: _is_verbose(true) 
		, _is_warnings_as_errors_enabled(false) {
	}

	void fbx_converter::reset_internals() {
		_mesh_def.reset();
		_mesh_data = fbx_converter_mesh_data();
		_unduped_vertices.clear();
	}

	std::shared_ptr<mesh_def> fbx_converter::convert_fbx_to_mesh_def(std::string path) {
		
		reset_internals();

		FbxManager* manager = FbxManager::Create();
		FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
		manager->SetIOSettings(ios);

		FbxImporter* importer = FbxImporter::Create(manager, "");
		if (!importer->Initialize(path.c_str(), -1, manager->GetIOSettings())) {
			ALERT("FbxImporter::Initialize error : {}", importer->GetStatus().GetErrorString());
		}
		else {
			FbxScene* scene = FbxScene::Create(manager, "");
			importer->Import(scene);

			FbxGeometryConverter geometry_converter(manager);
			if (!geometry_converter.Triangulate(scene, true)) {
				add_error_message("Failed to triangulate scene.");
			}

			find_and_process_fbx_mesh(scene);
			
			scene->Destroy();
		}

		importer->Destroy();
		manager->Destroy();

		return _mesh_def;
	}

	void fbx_converter::find_and_process_fbx_mesh(FbxScene* scene) {
		std::vector<FbxMesh*> fbx_meshes;
		find_fbx_meshes_recursive(fbx_meshes, scene->GetRootNode());
		if (fbx_meshes.size() == 0) {
			add_error_message("No Mesh Node found in Scene.");
		}
		else {
			if (fbx_meshes.size() > 1) {
				add_warning_message("Multiple Mesh Nodes found in Scene. Ignoring exta nodes.");
			}
			process_fbx_mesh(fbx_meshes.at(0));
		}
	}

	void fbx_converter::find_fbx_meshes_recursive(std::vector<FbxMesh*>& meshes, FbxNode* node) {
		auto attribute = node->GetNodeAttribute();
		if (attribute != nullptr && attribute->GetAttributeType() == FbxNodeAttribute::eMesh) {
			meshes.push_back(FbxCast<FbxMesh>(attribute));
		}

		for (int i = 0; i < node->GetChildCount(); ++i) {
			find_fbx_meshes_recursive(meshes, node->GetChild(i));
		}
	}

	void fbx_converter::process_fbx_mesh(FbxMesh* mesh) {
		build_mesh_data(mesh);
		handle_missing_mesh_data();
		build_unduped_vertices();
		sort_polygons_by_material_index();
		build_mesh_def();
	}

	void fbx_converter::build_mesh_data(FbxMesh* mesh) {
		ASSERT(_mesh_data._polygons.empty());
		ASSERT(_mesh_data._polygon_vertices.empty());
		ASSERT(_mesh_data._control_points.empty());

		_mesh_data._materials.reserve(mesh->GetNode()->GetMaterialCount());
		_mesh_data._polygons.reserve(mesh->GetPolygonCount());
		_mesh_data._polygon_vertices.reserve(mesh->GetPolygonVertexCount());
		_mesh_data._control_points.reserve(mesh->GetControlPointsCount());

		for (int i_material = 0; i_material < mesh->GetNode()->GetMaterialCount(); ++i_material) {
			auto material = make_mesh_data_material(mesh->GetNode()->GetMaterial(i_material));
			add_verbose_message(build_string("found material : {}", material.to_string()));
			_mesh_data._materials.push_back(material);
		}

		for (int i_control_point = 0; i_control_point < mesh->GetControlPointsCount(); ++i_control_point) {
			auto cp = std::make_shared<fbx_converter_mesh_data::control_point>();
			_mesh_data._control_points.push_back(cp);
		}

		add_verbose_message(build_string("found {} polygons", mesh->GetPolygonCount()));
		for (int i_polygon = 0; i_polygon < mesh->GetPolygonCount(); ++i_polygon) {
			if (mesh->GetPolygonSize(i_polygon) != 3) {
				add_error_message(build_string("Polygon of size {} found. Only triangles are supported.", mesh->GetPolygonSize(i_polygon)));
				break;
			}

			auto new_polygon = std::make_shared<fbx_converter_mesh_data::polygon>();
			_mesh_data._polygons.push_back(new_polygon);

			for (int i_polygon_vertex = 0; i_polygon_vertex < mesh->GetPolygonSize(i_polygon); ++i_polygon_vertex) {
				auto pv = std::make_shared<fbx_converter_mesh_data::polygon_vertex>();
				int control_point_index = mesh->GetPolygonVertex(i_polygon, i_polygon_vertex);
				pv->_data._position = convert_fbx_to_vec3(mesh->GetControlPointAt(control_point_index));

				_mesh_data._polygon_vertices.push_back(pv);
				new_polygon->_vertices.at(i_polygon_vertex) = pv;
				_mesh_data._control_points.at(control_point_index)->_vertices.push_back(pv);
			}
		}

		process_mesh_element_per_polygon_vertex<FbxLayerElementNormal, FbxVector4>(
			mesh,
			"ElementNormal",
			[](FbxMesh* mesh) { return mesh->GetElementNormalCount(); },
			[](FbxMesh* mesh, int i_element) { return mesh->GetElementNormal(i_element); },
			[&](fbx_converter_mesh_data::polygon_vertex* vertex, FbxVector4 value) {
				if (vertex->_data._normal.has_value()) {
					add_error_message("Vertex already has Normal!");
				}
				else {
					vertex->_data._normal = convert_fbx_to_vec3(value);
				}
			});

		process_mesh_element_per_polygon_vertex<FbxLayerElementTangent, FbxVector4>(
			mesh,
			"ElementTangent",
			[](FbxMesh* mesh) { return mesh->GetElementTangentCount(); },
			[](FbxMesh* mesh, int i_element) { return mesh->GetElementTangent(i_element); },
			[&](fbx_converter_mesh_data::polygon_vertex* vertex, FbxVector4 value) {
				if (vertex->_data._tangent.has_value()) {
					add_error_message("Vertex already has Tangent!");
				}
				else {
					vertex->_data._tangent = convert_fbx_to_vec3(value);
				}
			});

		process_mesh_element_per_polygon_vertex<FbxLayerElementUV, FbxVector2>(
			mesh,
			"ElementUV",
			[](FbxMesh* mesh) { return mesh->GetElementUVCount(FbxLayerElement::eTextureDiffuse); },
			[](FbxMesh* mesh, int i_element) { return mesh->GetElementUV(i_element, FbxLayerElement::eTextureDiffuse); },
			[&](fbx_converter_mesh_data::polygon_vertex* vertex, FbxVector2 value) {
				if (vertex->_data._uv.has_value()) {
					add_error_message("Vertex already has UV!");
				}
				else {
					vertex->_data._uv = convert_fbx_to_uv(value);
				}
			});

		process_mesh_element_per_polygon_with_index_to_direct<FbxLayerElementMaterial>(
			mesh,
			"ElementMaterial",
			[](FbxMesh* mesh) { return mesh->GetElementMaterialCount(); },
			[](FbxMesh* mesh, int i_element) { return mesh->GetElementMaterial(i_element); },
			[&](fbx_converter_mesh_data::polygon* polygon, int index_to_direct) {
				if (polygon->_material_index.has_value()) {
					add_error_message("Polygon already has MaterialIndex!");
				}
				else {
					polygon->_material_index = index_to_direct;
				}
			});
	}

	fbx_converter_mesh_data::material fbx_converter::make_mesh_data_material(FbxSurfaceMaterial* in_material) {

		fbx_converter_mesh_data::material out_material;

		//http://docs.autodesk.com/FBX/2014/ENU/FBX-SDK-Documentation/index.html
		//ImportScene/DisplayMaterial.cxx

		if (in_material->GetClassId().Is(FbxSurfacePhong::ClassId)) {
			auto phong = static_cast<FbxSurfacePhong*>(in_material);
			out_material._diffuse_map_file_name = get_texture_file_name(phong->Diffuse.GetSrcObject<FbxFileTexture>(), "DIFFUSE");
			out_material._normal_map_file_name = get_texture_file_name(phong->NormalMap.GetSrcObject<FbxFileTexture>(), "NORMAL_MAP");
		}
		else if (in_material->GetClassId().Is(FbxSurfaceLambert::ClassId)) {
			auto lambert = static_cast<FbxSurfaceLambert*>(in_material);
			out_material._diffuse_map_file_name = get_texture_file_name(lambert->Diffuse.GetSrcObject<FbxFileTexture>(), "DIFFUSE");
			out_material._normal_map_file_name = get_texture_file_name(lambert->NormalMap.GetSrcObject<FbxFileTexture>(), "NORMAL_MAP");
		}
		else {
			add_error_message(build_string("Unknown Material class type : {}", in_material->GetName()));
		}

		return out_material;
	}

	std::string fbx_converter::get_texture_file_name(FbxFileTexture* fbx_file_texture, const char* texture_type) {
		if (fbx_file_texture == nullptr) {
			add_error_message(build_string("No {} texture found on material.", texture_type));
			return "";
		}
		return fbx_file_texture->GetFileName();
	}

	void fbx_converter::handle_missing_mesh_data() {
		
		if (_mesh_data._materials.empty()) {
			add_error_message("No materials found");
			_mesh_data._materials.push_back(fbx_converter_mesh_data::material()); //add a dummy material so material indices can always be valid.
		}

		int no_normal_count = 0;
		int no_tangent_count = 0;
		int no_uv_count = 0;

		for (auto pv : _mesh_data._polygon_vertices) {
			if (!pv->_data._normal.has_value()) {
				no_normal_count++;
				pv->_data._normal = vec3(1.f, 0.f, 0.f);
			}

			if (!pv->_data._tangent.has_value()) {
				no_tangent_count++;
				pv->_data._tangent = vec3(1.f, 0.f, 0.f);
			}

			if (!pv->_data._uv.has_value()) {
				no_uv_count++;
				pv->_data._tangent = vec3(1.f, 0.f, 0.f);
			}
		}

		if (no_normal_count > 0) {
			add_warning_message(build_string("{} vertices are missing normals", no_normal_count));
		}
		if (no_tangent_count > 0) {
			add_warning_message(build_string("{} vertices are missing tangents", no_tangent_count));
		}
		if (no_uv_count > 0) {
			add_warning_message(build_string("{} vertices are missing uvs", no_uv_count));
		}

		int no_material_count = 0;

		for (auto p : _mesh_data._polygons) {
			if (!p->_material_index.has_value()) {
				no_material_count++;
				p->_material_index = 0;
			}
			else if (p->_material_index.value() >= _mesh_data._materials.size()) {
				add_error_message(build_string("Polygon Material Index is invalid : {}", p->_material_index.value()));
				p->_material_index = 0;
			}
		}

		if (no_material_count > 0) {
			add_warning_message("{} polygons are missing material index");
		}
	}

	void fbx_converter::build_unduped_vertices() {
		//want all vertices that have the exact same data (position,normal,etc) to not be duplicated.
		ASSERT(_unduped_vertices.empty());

		std::unordered_map<checksum, std::shared_ptr<fbx_converter_mesh_data::polygon_vertex>> checksum_map;

		for (auto v : _mesh_data._polygon_vertices) {
			auto checksum = v->_data.get_checksum();

			bool is_dup = false;
			auto dup_iter = checksum_map.find(checksum);
			if (dup_iter != checksum_map.end()) {
				//potential duplicate vertex
				is_dup = (v->_data == dup_iter->second->_data);
				if (!is_dup) {
					add_warning_message("false positive duplicate vertex checksum detected.");
				}
			}
			else {
				checksum_map[checksum] = v;
			}

			if (is_dup) {
				v->_unduped_vertex_index = dup_iter->second->_unduped_vertex_index;
			}
			else {
				_unduped_vertices.push_back(v->_data);
				v->_unduped_vertex_index = _unduped_vertices.size() - 1;
			}
		}

		add_verbose_message(build_string("found {} unique vertices", _unduped_vertices.size()));
	}

	void fbx_converter::sort_polygons_by_material_index() {
		//want all triangles with the same material index grouped together to reduce render state changes.
		std::sort(
			std::begin(_mesh_data._polygons), 
			std::end(_mesh_data._polygons),
			[](std::shared_ptr<fbx_converter_mesh_data::polygon> a, std::shared_ptr<fbx_converter_mesh_data::polygon> b) {
				return a->_material_index.value() < b->_material_index.value();
			});
	}

	void fbx_converter::build_mesh_def() {
		auto md = std::make_shared<mesh_def>();

		for (auto material : _mesh_data._materials) {
			mesh_material mat;
			mat._diffuse_map = get_file_name_no_path_no_extension(material._diffuse_map_file_name);
			mat._normal_map = get_file_name_no_path_no_extension(material._normal_map_file_name);
			md->_materials.push_back(mat);
		}

		for (auto polygon : _mesh_data._polygons) {
			mesh_triangle tri;
			//NOTE: reverse winding order due to RH->LH coordinate system.
			tri._vertex_index_0 = int_to_ushort(polygon->_vertices.at(0)->_unduped_vertex_index.value());
			tri._vertex_index_1 = int_to_ushort(polygon->_vertices.at(2)->_unduped_vertex_index.value());
			tri._vertex_index_2 = int_to_ushort(polygon->_vertices.at(1)->_unduped_vertex_index.value());
			tri._material_index = int_to_ushort(polygon->_material_index.value());
			md->_triangles.push_back(tri);
		}

		for (auto vertex : _unduped_vertices) {
			mesh_vertex mesh_vertex;
			mesh_vertex._position = vertex._position.value();
			mesh_vertex._normal = vertex._normal.value();
			mesh_vertex._tangent = vertex._tangent.value();
			mesh_vertex._uv = vertex._uv.value();
			md->_vertices.push_back(mesh_vertex);
		}

		_mesh_def = md;
	}

	void fbx_converter::add_verbose_message(const std::string& message) {
		if (_is_verbose) {
			TRACE(message.c_str());
		}
	}

	void fbx_converter::add_warning_message(const std::string& message) {
		if (_is_warnings_as_errors_enabled) {
			ALERT(message.c_str());
		}
		else {
			TRACE("WARNING : {}", message);
		}
	}

	void fbx_converter::add_error_message(const std::string& message) {
		ALERT(message.c_str());
	}

	vec3 fbx_converter::convert_fbx_to_vec3(const FbxVector4& v) {
		//RH->LH
		return make_mat33_rotation_on_y(deg(180.f)).transform_vec3(
			vec3(
				double_to_float(v[0]), 
				double_to_float(v[1]), 
				-double_to_float(v[2])));
	}

	uv fbx_converter::convert_fbx_to_uv(const FbxVector2& v) {
		//RH->LH
		return uv(
			double_to_float(v[0]), 
			double_to_float(1.0 - v[1]));
	}

}