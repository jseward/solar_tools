#pragma once

#include <fbxsdk.h>

namespace solar {

	extern const char* fbx_mapping_mode_to_string(FbxLayerElement::EMappingMode mapping_mode);
	extern const char* fbx_reference_mode_to_string(FbxLayerElement::EReferenceMode reference_mode);

}