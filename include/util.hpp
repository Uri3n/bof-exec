#ifndef UTIL_HPP
#define UTIL_HPP
#include <string>
#include <vector>
#include <fstream>
#include <optional>
#include <iostream>

bool pack_arguments(std::string unpacked, std::vector<char>& packed);
std::optional<std::vector<char>> read_from_disk(const std::string& file_name);

template<typename T>
class defer_wrapper {
    T callable;
public:
    auto call() -> decltype(callable()) {
        return callable();
    }

    explicit defer_wrapper(T func) : callable(func) {}
    ~defer_wrapper() { callable(); }
};

template<typename T>
defer_wrapper<T> defer(T callable) {
    return defer_wrapper<T>(callable);
}


#endif //UTIL_HPP
