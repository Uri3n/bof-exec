#ifndef STRUCTS_HPP
#define STRUCTS_HPP
#include <Windows.h>
#include <string>

struct section_map {
    PVOID base;
    ULONG size;
};

struct object_context {
    union {
        ULONG_PTR          base;
        PIMAGE_FILE_HEADER header;
    };

    PIMAGE_SYMBOL       sym_table;
    PVOID*              sym_map;
    section_map*        sec_map;
    PIMAGE_SECTION_HEADER sections;
};

struct beacon_function_pair { //unused.
    std::string name;
    void* func = nullptr;
};

#endif //STRUCTS_HPP
