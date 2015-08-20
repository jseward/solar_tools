#include "fbx_enum_helpers.h"
#include "solar/utility/assert.h"

namespace solar {

	const char* fbx_mapping_mode_to_string(FbxLayerElement::EMappingMode mapping_mode) {
		switch (mapping_mode) {
		case FbxLayerElement::eNone: return "eNone";
		case FbxLayerElement::eByControlPoint: return "eByControlPoint";
		case FbxLayerElement::eByPolygonVertex: return "eByPolygonVertex";
		case FbxLayerElement::eByPolygon: return "eByPolygon";
		case FbxLayerElement::eByEdge: return "eByEdge";
		case FbxLayerElement::eAllSame: return "eAllSame";
		default: ASSERT(false);
		}
		return "???";
	}

	const char* fbx_reference_mode_to_string(FbxLayerElement::EReferenceMode reference_mode) {
		switch (reference_mode) {
		case FbxLayerElement::eDirect: return "eDirect";
		case FbxLayerElement::eIndex: return "eIndex";
		case FbxLayerElement::eIndexToDirect: return "eIndexToDirect";
		default: ASSERT(false);
		}
		return "???";
	}

}