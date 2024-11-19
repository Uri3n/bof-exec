#ifndef MACRO_HPP
#define MACRO_HPP

#define SIZE_OF_PAGE     0x1000
#define PAGE_ALIGN(x)    (((uint64_t)x) + ((SIZE_OF_PAGE - (((uint64_t)x) & (SIZE_OF_PAGE - 1))) % SIZE_OF_PAGE))
#define PTR_TO_U64(ptr)  reinterpret_cast<uint64_t>(ptr)
#define INT_TO_U64(x)    static_cast<uint64_t>(x)

#endif //MACRO_HPP
