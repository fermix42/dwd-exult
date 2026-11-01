#pragma once
#include <string>
extern bool        cli_enabled;
extern std::string cli_run_dir;
void               cli_start();
int                cli_poll(bool input, void* unused);
bool               cli_validate_paths();
void               cli_require_write_path(const std::string&);
bool               cli_config_isolated(const std::string&);
void               cli_cleanup_complete(bool cancelled);
extern void*       cli_input_paint;

struct Cli_input_scope {
	void* saved;

	explicit Cli_input_scope(void* p) : saved(cli_input_paint) {
		if (cli_enabled) {
			cli_input_paint = p;
		}
	}

	~Cli_input_scope() {
		if (cli_enabled) {
			cli_input_paint = saved;
		}
	}
};
