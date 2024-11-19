#include <BOF-exec.hpp>

uint32_t object_virtual_size(object_context* ctx)
{
    PIMAGE_RELOCATION obj_rel  = nullptr;
    PIMAGE_SYMBOL obj_sym      = nullptr;
    char* symbol_name          = nullptr;
    uint32_t total_size        = 0;

    //
    // Add up each page aligned section size.
    //
    for (size_t i = 0; i < ctx->header->NumberOfSections; i++) {
        total_size += PAGE_ALIGN(ctx->sections[i].SizeOfRawData);
    }

    for (size_t i = 0; i < ctx->header->NumberOfSections; i++) {
        obj_rel = reinterpret_cast<PIMAGE_RELOCATION>(ctx->base + ctx->sections[i].PointerToRelocations);
        for (size_t j = 0; j < ctx->sections[i].NumberOfRelocations; j++) {
            obj_sym = &ctx->sym_table[obj_rel->SymbolTableIndex];

            //
            // get symbol name
            //
            if (obj_sym->N.Name.Short) {
                symbol_name = reinterpret_cast<char*>(obj_sym->N.ShortName); // short name (> 8 bytes)
            } else {
                symbol_name = reinterpret_cast<char*>(PTR_TO_U64(ctx->sym_table + ctx->header->NumberOfSymbols) + INT_TO_U64(obj_sym->N.Name.Long));
            }

            //
            // Only if the symbol is external
            //
            if (strncmp("__imp_", symbol_name, 6) == 0) {
                total_size += sizeof(void*);
            }
            obj_rel = reinterpret_cast<PIMAGE_RELOCATION>(PTR_TO_U64(obj_rel) + sizeof(IMAGE_RELOCATION));
        }
    }

    return PAGE_ALIGN(total_size); // align the size to a page boundary on return
}

bool object_execute(object_context* ctx, const char* entry, char* args, const uint32_t argc)
{
    void (*main)(char*, uint32_t)  = nullptr;
    PIMAGE_SYMBOL symbol           = nullptr;
    char* symbol_name              = nullptr;
    void* section_base             = nullptr;
    uint32_t section_size          = 0;
    uint32_t old_protect           = 0;

    for (size_t i = 0; i < ctx->header->NumberOfSymbols; i++) {
        symbol = &ctx->sym_table[i];
        if (symbol->N.Name.Short) { // if the symbol has a short name
            symbol_name = reinterpret_cast<char*>(symbol->N.ShortName);
        } else {
            symbol_name = reinterpret_cast<char*>(PTR_TO_U64(ctx->sym_table + ctx->header->NumberOfSymbols) + INT_TO_U64(symbol->N.Name.Long));
        }

        if (ISFCN(ctx->sym_table[i].Type) && strcmp(entry, symbol_name) == 0) {
            section_base = ctx->sec_map[symbol->SectionNumber - 1].base;
            section_size = ctx->sec_map[symbol->SectionNumber - 1].size;

            //
            // Change the section where the symbol is to R/X
            //
            if (!VirtualProtect(
                 section_base,
                 section_size,
                 PAGE_EXECUTE_READ,
                 reinterpret_cast<PDWORD>(&old_protect)
            )) {
                return false;
            }

            //
            // Call the function
            //
            main = reinterpret_cast<decltype(main)>(PTR_TO_U64(section_base) + symbol->Value);
            main(args, argc);

            //
            // Restore previous protection
            //
            if (!VirtualProtect(
                 section_base,
                 section_size,
                 old_protect,
                 reinterpret_cast<PDWORD>(&old_protect)
            )) {
                return false;
            }

            return true;
        }
    }

    return false;
}

void* resolve_object_symbol(const char* symbol)
{
    std::string function;
    std::string library;
    void* resolved_func = nullptr;

    static const std::map<std::string, void*> api_pairs = {
        { "BeaconOutput", BeaconOutput },
        { "BeaconPrintf", BeaconPrintf },
        { "BeaconDataParse", BeaconDataParse },
        { "BeaconDataInt", BeaconDataInt },
        { "BeaconDataShort", BeaconDataShort },
        { "BeaconDataLength", BeaconDataLength },
        { "BeaconDataExtract", BeaconDataExtract },
        { "BeaconIsAdmin", BeaconIsAdmin },
        { "BeaconUseToken", BeaconUseToken },
        { "BeaconRevertToken", BeaconRevertToken },
        { "BeaconFormatAlloc", BeaconFormatAlloc },
        { "BeaconFormatReset", BeaconFormatReset },
        { "BeaconFormatAppend", BeaconFormatAppend },
        { "BeaconFormatPrintf", BeaconFormatPrintf },
        { "BeaconFormatToString", BeaconFormatToString },
        { "BeaconFormatFree", BeaconFormatFree },
        { "BeaconFormatInt", BeaconFormatInt },
        { "BeaconGetSpawnTo", BeaconGetSpawnTo },
        { "BeaconSpawnTemporaryProcess", BeaconSpawnTemporaryProcess },
        { "BeaconInjectTemporaryProcess", BeaconInjectTemporaryProcess },
        { "BeaconInjectProcess", BeaconInjectProcess },
        { "BeaconCleanupProcess", BeaconCleanupProcess },
        { "toWideChar", toWideChar },
    };

    if (symbol == nullptr || strncmp("__imp_", symbol, 6) != 0) {
        return nullptr;
    }

    symbol += 6; // move past the "__imp_" string

    //
    // if the symbol is a Beacon API function, check which one it is.
    //
    if (strncmp("Beacon", symbol, 6) == 0) {

        if (const auto found = api_pairs.find(symbol); found != api_pairs.end()) {
            resolved_func = found->second;
        } else {
            clear_beacon_output();
            manip_beacon_output((char*)(std::string("Unsupported beacon function: ") + symbol).c_str(), false, false, nullptr);
            return nullptr;
        }
    }

    //
    // otherwise we need to resolve the symbol via GetProcAddress/LoadLibrary
    //
    else {
        const std::string obj = symbol;
        const size_t pos = obj.find_first_of('$');
        HMODULE hmod = nullptr;

        if (pos == std::string::npos) {
            return nullptr;
        }

        library = obj.substr(0, pos);
        function = obj.substr(pos + 1);

        if (!(hmod = GetModuleHandleA(library.c_str())) && !(hmod = LoadLibraryA(library.c_str()))) {
            return nullptr;
        }

        resolved_func = GetProcAddress(hmod, function.c_str());
        if (!resolved_func) {
            return nullptr;
        }
    }

    return resolved_func;
}

void object_relocation(const uint32_t type, void* needs_relocating, void* section_base)
{
    switch (type) {
    case IMAGE_REL_AMD64_REL32:
        *(uint32_t*)needs_relocating = (*(uint32_t*)(needs_relocating)) + (unsigned long)(PTR_TO_U64(section_base) - PTR_TO_U64(needs_relocating) - sizeof(UINT32));
        break;
    case IMAGE_REL_AMD64_REL32_1:
        *(uint32_t*)needs_relocating = (*(uint32_t*)(needs_relocating)) + (unsigned long)(PTR_TO_U64(section_base) - PTR_TO_U64(needs_relocating) - sizeof(UINT32) - 1);
        break;
    case IMAGE_REL_AMD64_REL32_2:
        *(uint32_t*)needs_relocating = (*(uint32_t*)(needs_relocating)) + (unsigned long)(PTR_TO_U64(section_base) - PTR_TO_U64(needs_relocating) - sizeof(UINT32) - 2);
        break;
    case IMAGE_REL_AMD64_REL32_3:
        *(uint32_t*)needs_relocating = (*(uint32_t*)(needs_relocating)) + (unsigned long)(PTR_TO_U64(section_base) - PTR_TO_U64(needs_relocating) - sizeof(UINT32) - 3);
        break;
    case IMAGE_REL_AMD64_REL32_4:
        *(uint32_t*)needs_relocating = (*(uint32_t*)(needs_relocating)) + (unsigned long)(PTR_TO_U64(section_base) - PTR_TO_U64(needs_relocating) - sizeof(UINT32) - 4);
        break;
    case IMAGE_REL_AMD64_REL32_5:
        *(uint32_t*)needs_relocating = (*(uint32_t*)(needs_relocating)) + (unsigned long)(PTR_TO_U64(section_base) - PTR_TO_U64(needs_relocating) - sizeof(UINT32) - 5);
        break;
    case IMAGE_REL_AMD64_ADDR64:
        *(uint64_t*)needs_relocating = (*(uint64_t*)(needs_relocating)) + PTR_TO_U64(section_base);
        break;
    default:
        break;
    }
}

bool process_object_sections(object_context* ctx)
{
    void* section_base             = nullptr;
    void* resolved_addr            = nullptr;
    void* needs_resolving          = nullptr;
    uint32_t func_index            = 0;
    PIMAGE_RELOCATION relocation   = nullptr;
    PIMAGE_SYMBOL symbol           = nullptr;
    char* symbol_name              = nullptr;

    //---------------------------------------------------//

    for (size_t i = 0; i < ctx->header->NumberOfSections; i++) {
        relocation = reinterpret_cast<PIMAGE_RELOCATION>(ctx->base + ctx->sections[i].PointerToRelocations);

        //
        // iterate over each relocation entry for the section.
        //
        for (size_t j = 0; j < ctx->sections[i].NumberOfRelocations; j++) {
            symbol = &ctx->sym_table[relocation->SymbolTableIndex];

            if (symbol->N.Name.Short) {
                symbol_name = reinterpret_cast<char*>(symbol->N.ShortName);
            } else {
                symbol_name = reinterpret_cast<char*>(PTR_TO_U64(ctx->sym_table + ctx->header->NumberOfSymbols) + (symbol->N.Name.Long));
            }

            //
            // RVA for the relocation needs to be applied to the base of the section.
            //
            needs_resolving = reinterpret_cast<void*>(PTR_TO_U64(ctx->sec_map[i].base) + relocation->VirtualAddress);
            resolved_addr = nullptr;

            if (strncmp("__imp_", symbol_name, 6) == 0) {
                resolved_addr = resolve_object_symbol(symbol_name);
                if (resolved_addr == nullptr) {
                    return false;
                }
            }

            if (relocation->Type == IMAGE_REL_AMD64_REL32 && resolved_addr) { // Relative 32-bit relocation for jumps/calls
                ctx->sym_map[func_index] = resolved_addr;
                *((uint32_t*)needs_resolving) = static_cast<uint32_t>((PTR_TO_U64(ctx->sym_map) + func_index * sizeof(void*)) - PTR_TO_U64(needs_resolving) - sizeof(uint32_t));
                func_index++;
            } else {
                section_base = ctx->sec_map[symbol->SectionNumber - 1].base;
                object_relocation(relocation->Type, needs_resolving, section_base);
            }

            relocation = reinterpret_cast<PIMAGE_RELOCATION>(PTR_TO_U64(relocation) + sizeof(IMAGE_RELOCATION));
        }
    }

    return true;
}

bool load_object(
    void* pobject,
    const std::string& func_name,
    char* arguments,
    const uint32_t argc)
{

    object_context ctx = { 0 };
    uint32_t virtual_size = 0;
    uint32_t section_size = 0;
    void* virtual_addr = nullptr;
    void* section_base = nullptr;

    //------------------------------------//

    auto _ = defer([&]() {
        if (virtual_addr != nullptr) {
            VirtualFree(virtual_addr, 0, MEM_RELEASE);
        }
        if (ctx.sec_map != nullptr) {
            HeapFree(GetProcessHeap(), 0, ctx.sec_map);
            ctx.sec_map = nullptr;
        }
    });

    if (!pobject || func_name.empty()) {
        return false;
    }

    ctx.header = static_cast<PIMAGE_FILE_HEADER>(pobject);
    ctx.sym_table = reinterpret_cast<PIMAGE_SYMBOL>(PTR_TO_U64(pobject) + ctx.header->PointerToSymbolTable);
    ctx.sections = reinterpret_cast<PIMAGE_SECTION_HEADER>(PTR_TO_U64(pobject) + sizeof(IMAGE_FILE_HEADER));

    if (ctx.header->Machine != IMAGE_FILE_MACHINE_AMD64) { // do not support 32 bit
        return false;
    }

    //
    // allocate memory
    //
    virtual_size = object_virtual_size(&ctx);
    virtual_addr = VirtualAlloc(
        nullptr,
        virtual_size,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);

    if (virtual_addr == nullptr) {
        return false;
    }

    ctx.sec_map = static_cast<section_map*>(HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        ctx.header->NumberOfSections * sizeof(section_map)));

    if (ctx.sec_map == nullptr) {
        return false;
    }

    //
    // copy over sections from the object file
    //
    section_base = virtual_addr;
    for (size_t i = 0; i < ctx.header->NumberOfSections; i++) {

        section_size = ctx.sections[i].SizeOfRawData;
        ctx.sec_map[i].size = section_size;
        ctx.sec_map[i].base = section_base;

        memcpy( // copy over the section
            section_base,
            reinterpret_cast<void*>(ctx.base + ctx.sections[i].PointerToRawData),
            section_size);

        section_base = reinterpret_cast<void*>(PAGE_ALIGN(PTR_TO_U64(section_base) + section_size));
    }

    //
    // Process COFF sections
    //
    ctx.sym_map = static_cast<PVOID*>(section_base);
    if (!process_object_sections(&ctx)) {
        return false;
    }

    //
    // Execute "go"
    //
    if (!object_execute(&ctx, func_name.c_str(), arguments, argc)) {
        return false;
    }

    return true;
}

void sig_handle_ctrlc(int signal)
{
    std::cout << std::endl;
    std::cout << "[*] Keyboard interrupt received. Exiting..." << std::endl;
    std::exit(signal);
}

int main(int argc, char** argv)
{
    std::signal(SIGINT, sig_handle_ctrlc);
    std::cout <<
        R"(
 _            __
| |__   ___  / _|       _____  _____  ___
| '_ \ / _ \| |_ _____ / _ \ \/ / _ \/ __|
| |_) | (_) |  _|_____|  __/>  <  __/ (__
|_.__/ \___/|_|        \___/_/\_\___|\___|
)" << std::endl;

    if (argc < 2) {
        std::cout << R"(  Usage: [INPUT FILE] [ARGUMENTS (optional)])" << std::endl;
        std::cout << R"(  Examples: BOF-exec bof.o "string argument, i32, i200")" << std::endl;
        std::cout << R"(            BOF-exec bof.obj "i16, s-50, s121")" << std::endl;
        std::cout << R"(            BOF-exec bof.o)" << std::endl
                  << std::endl;

        std::cout << R"(  Note: passing arguments to the "go" function is optional.)" << std::endl;
        std::cout << R"(   - arguments can be passed as integers by prefixing the argument with "i" or "s".)" << std::endl;
        std::cout << R"(   - "i" passes the argument as a 32 bit integer, and "s" passes a 16 bit one.)" << std::endl;
        std::cout << R"(   - integer arguments can be negative numbers, such as: "i-32" or "s-2")" << std::endl;
        return EXIT_FAILURE;
    }

    if (!std::filesystem::exists(argv[1]) || (std::filesystem::path(argv[1]).extension().string() != ".o" && // only permit .o or .obj
            std::filesystem::path(argv[1]).extension().string() != ".obj")
    ) {
        std::cerr << "[!] ERROR, Input file does not exist, or is not an object file." << std::endl;
        return EXIT_FAILURE;
    }

    auto input_file = read_from_disk(argv[1]);
    if (!input_file) {
        return EXIT_FAILURE;
    }

    std::vector<char> packed;
    if (argc > 2) {
        if (!pack_arguments(argv[2], packed)) {
            std::cerr << "[!] ERROR, invalid BOF arguments passed." << std::endl;
            return EXIT_FAILURE;
        }
    }

    std::cout << "[*] Executing object file: " << argv[1] << "..." << std::endl;
    std::cout << "[*] Arguments provided: " << (argc > 2 ? argv[2] : "None") << std::endl;
    std::cout << "[*] Argument size (packed): " << packed.size() << std::endl;

    if (!load_object(
         input_file->data(),
         "go",
         packed.empty() ? nullptr : packed.data(),
         packed.size())
    ){
        std::cerr << "[!] ERROR, failed to execute BOF." << std::endl;
        return EXIT_FAILURE;
    }

    const std::string output = get_beacon_output();
    std::cout << "[*] BOF output:"
              << "\n\n";
    std::cout << output << "\n\n";
    std::cout << "[+] Finished executing BOF." << std::endl;

    return EXIT_SUCCESS;
}
