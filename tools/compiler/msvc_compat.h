// Standalone UCC host-tool support for MSVC (not included by the engine).
#pragma once
#include <io.h>
#include <cstdio>
#include <cstring>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

inline int optind = 1, opterr = 0, optopt = 0;
inline char* optarg = nullptr;
inline int getopt(int argc, char** argv, const char* spec) {
    static const char* next = nullptr;
    if (!next || !*next) {
        if (optind >= argc || argv[optind][0] != '-' || !argv[optind][1]) return -1;
        if (std::strcmp(argv[optind], "--") == 0) { ++optind; return -1; }
        next = argv[optind++] + 1;
    }
    optopt = *next++;
    const char* option = std::strchr(spec, optopt);
    if (!option || optopt == ':') return '?';
    optarg = nullptr;
    if (option[1] == ':') {
        if (*next) { optarg = const_cast<char*>(next); next = nullptr; }
        else if (optind < argc) { optarg = argv[optind++]; next = nullptr; }
        else return spec[0] == ':' ? ':' : '?';
    }
    return optopt;
}
