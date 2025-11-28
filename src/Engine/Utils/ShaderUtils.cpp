#include "Engine/Utils/ShaderUtils.h"
#include <fstream>

std::vector<uint32_t> load_spv(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open SPV: %s\n", path.c_str());
        return {};
    }

    size_t size = (size_t)file.tellg();
    file.seekg(0);
    std::vector<uint32_t> buffer((size + 3) / 4);
    file.read((char*)buffer.data(), size);
    file.close();
    return buffer;
}