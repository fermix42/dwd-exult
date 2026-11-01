#pragma once
#include <sstream>
#include <string>
std::string cli_quote(const std::string&);
std::string cli_world(int sequence, const std::string& op, int x, int y, std::istringstream& in, bool input);
void        cli_world_boundary();
void        cli_world_reset();
void        cli_reply(int, bool, const std::string&);
