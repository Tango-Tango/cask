#include "cask/Pool.hpp"
#include <cstring>

namespace cask {

Pool::Pool()
    : memory_pool()
    , free_list()
{
    memory_pool = new uint8_t*[num_blocks];

    for(std::size_t i = 0; i < num_blocks; i++) {
        memory_pool[i] = new uint8_t[block_size];
        free_list.push(i);
    }
}

Pool::~Pool() {
    for(std::size_t i = 0; i < num_blocks; i++) {
        delete [] memory_pool[i];
    }

    delete [] memory_pool;
}

}