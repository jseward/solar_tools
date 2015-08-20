#pragma once

#include "solar/utility/optional.h"
#include "solar/utility/checksum.h"

namespace solar {

	class fbx_polygon_vertex_data {
	public:
		optional<vec3> _position;
		optional<vec3> _normal;
		optional<vec3> _tangent;
		optional<uv> _uv;

	public:
		checksum get_checksum() const {
			return checksum()
				.add_checksum_at_index(0, checksum().add_vec3(_position.value()))
				.add_checksum_at_index(1, checksum().add_vec3(_normal.value_or_default()))
				.add_checksum_at_index(2, checksum().add_vec3(_tangent.value_or_default()))
				.add_checksum_at_index(3, _uv.value_or_default().to_checksum());
		}

		bool operator==(const fbx_polygon_vertex_data& rhs) const {
			return
				_position.value() == rhs._position.value() &&
				_normal.value() == rhs._normal.value() &&
				_tangent.value() == rhs._tangent.value() &&
				_uv.value() == rhs._uv.value();
		}
	};

}