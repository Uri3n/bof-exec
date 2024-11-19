#include <util.hpp>


std::optional<std::vector<char>>
read_from_disk(const std::string& file_name) {

    std::ifstream       input(file_name, std::ios::binary);
    std::vector<char>   out;

    //------------------------------------------------------//

    auto _ = defer([&]() {
       if(input.is_open()) {
           input.close();
       }
    });

    if(!input.is_open()) {
        std::cerr << "[!] ERROR, Failed to open input file: " << file_name << std::endl;
        return std::nullopt;
    }


    input.seekg(0, std::ios::end);
    const std::streamsize file_size = input.tellg();
    input.seekg(0, std::ios::beg);


    out.resize(file_size);
    if(!input.read(out.data(), file_size)) {
        std::cerr << "[!] ERROR, Failed to read file contents: " << file_name << std::endl;
        return std::nullopt;
    }


    std::cout << "[+] Read from disk: " << file_name << std::endl;
    return out;
}


bool
is_numeric(const std::string& str) {
    for( size_t i = 0; i < str.size(); i++ ) {
        if(i == 0 && str[i] == '-') {
            if(str.size() == 1) {
                return false;
            }
            continue;
        }
        if(!isdigit(str[i])) {
            return false;
        }
    }

    return true;
}


bool
is_ambiguous(const std::string& str) {
    if(!str.empty() && (str[0] == 's' || str[0] == 'i')) {
        if(str.size() == 1) {
            return true;
        } if(!is_numeric(std::string(&str.c_str()[1]))) {
            return true;
        }
    }

    return false;
}


void
strip_whitespace(std::string& str) {

    if (str.empty()) {
        return;
    }

    size_t first_non_space = str.find_first_not_of(" \t");
    if (first_non_space != std::string::npos) {
        str.erase(0, first_non_space);
    }

    if (str.empty()) {
        return;
    }


    if (str.back() == ' ' || str.back() == '\t') {
        size_t r_first_non_space = str.size() - 1;
        while ((r_first_non_space - 1) >= 0 && (str[r_first_non_space - 1] == ' ' || str[r_first_non_space - 1] == '\t')) {
            --r_first_non_space;
        }

        str.erase(r_first_non_space);
    }
}


bool
pack_arguments(std::string unpacked, std::vector<char>& packed) {

    std::vector<std::string>    chunks;
    size_t                      start    = 0;
    size_t                      next_arg = 0;


    if(unpacked.empty()) {
        return false;
    }

    if(unpacked.back() != ',') {
        unpacked += ',';
    }

    next_arg = unpacked.find(',');
    while(next_arg != std::string::npos) {
        if(next_arg > start) {
            chunks.push_back(unpacked.substr(start, next_arg - start));
        }

        start = next_arg + 1;
        next_arg = unpacked.find(',', start);
    }

    if(chunks.empty()) {
        return false;
    }


    for(std::string& str : chunks) {

        strip_whitespace(str);
        if(str.empty()) {
            return false;
        }

        if((str[0] != 's' && str[0] != 'i') || is_ambiguous(str) ) {
            auto                size_prefix = static_cast<uint32_t>(str.size() + 1); //std::string::size does not include the null terminator
            std::vector<char>   prefix_buffer;

            prefix_buffer.resize(sizeof(uint32_t));
            memcpy(prefix_buffer.data(), &size_prefix, sizeof(uint32_t));

            packed.insert(packed.end(), prefix_buffer.begin(), prefix_buffer.end());
            packed.insert(packed.end(), str.begin(), str.end());
            packed.emplace_back('\0');

            std::cout << "[+] Packed argument: " << str << " (" << size_prefix << " bytes)" << std::endl;
        }

        else {
            const bool          is_short = (str[0] == 's');
            std::vector<char>   buffer;

            buffer.resize(is_short ? sizeof(short) : sizeof(int));
            str.erase(0,1);

            if(str.empty() || !is_numeric(str)) {
                return false;
            }


            const int arg_int = atoi(str.c_str());
            if(!arg_int) {
                return false;
            }


            if(is_short) {
                short arg_short = static_cast<short>(arg_int);
                memcpy(buffer.data(), &arg_short, sizeof(short));
                std::cout << "[+] Packed argument: " << arg_short << " (int16)" << std::endl;
            } else {
                memcpy(buffer.data(), &arg_int, sizeof(int));
                std::cout << "[+] Packed argument: " << arg_int << " (int32)" << std::endl;
            }

            packed.insert(packed.end(), buffer.begin(), buffer.end());
        }
    }

    return true;
}