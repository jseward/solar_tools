#include <tchar.h>
#include "solar_engines/win32_cli_engine.h"
#include "solar/utility/command_line_parser.h"
#include "solar/utility/alert.h"
#include "solar/utility/trace.h"
#include "solar/io/file_stream_ptr.h"
#include "solar/strings/string_build.h"
#include "solar/archiving/json_archive_writer.h"
#include "solar/archiving/binary_archive_writer.h"
#include "solar/rendering/meshes/mesh_def.h"
#include "fbx_converter.h"

using namespace solar;

std::unique_ptr<archive_writer> make_writer(std::string format, stream& stream);

int _tmain(int argc, _TCHAR* argv[])
{
	win32_cli_engine engine;

	try {
		engine.setup(win32_cli_engine_setup_params()
			.set_app_params(win32_cli_app_setup_params()
				.set_alert_behavoir(win32_cli_app_error_behavoir::WRITE_AND_CONTINUE) //ALERT should not be exceptional here, main properly returns not 0. see get_error_count()
				.set_assert_behavoir(win32_cli_app_error_behavoir::THROW)));

		auto parser = command_line_parser()
			.add_required_value('i', "input", "input .fbx file")
			.add_required_value('o', "output", "output .mesh file")
			.add_optional_value('f', "format", "output format (json or binary)", "binary");

		if (!parser.execute(argc, argv)) {
			return 1;
		}

		fbx_converter converter;
		auto mesh_def = converter.convert_fbx_to_mesh_def(parser.get_value("input"));
		if (mesh_def != nullptr) {
			auto fs = make_file_stream_ptr(engine._win32_file_system, parser.get_value("output"), file_mode::CREATE_WRITE);
			auto writer = make_writer(parser.get_value("format"), *fs);
			writer->begin_writing();
			mesh_def->write_to_archive(*writer.get());
			writer->end_writing();
		}

		if (engine._win32_cli_app.get_error_count() > 0) {
			return engine._win32_cli_app.get_error_count();
		}

		engine.teardown();
	}
	catch (std::exception e) {
		engine._win32_cli_app.write_to_error_console(e.what());
 		return 1;
	}

	return 0;
}

std::unique_ptr<archive_writer> make_writer(std::string format, stream& stream) {
	if (format == "json") {
		return std::make_unique<json_archive_writer>(stream);
	}
	else if (format == "binary") {
		return std::make_unique<binary_archive_writer>(stream);
	}

	throw std::runtime_error(build_string("unknown export format : {}", format));
}

