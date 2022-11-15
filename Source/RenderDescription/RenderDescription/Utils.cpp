#include "Utils.h"

namespace Nerine
{

void SaveStringArray(std::ofstream& file, const std::vector<std::string>& arr)
{
    u32 size = arr.size();
    file.write((char*)&size, sizeof(size));
    for (const auto& s : arr)
    {
        size = (u32)s.length();
        file.write((char*)&size, sizeof(size));
        file.write(s.c_str(), size + 1);
    }
}

void LoadStringArray(std::ifstream& file, std::vector<std::string>& arr)
{
    u32 size = 0;
    file.read((char*)&size, sizeof(u32));
    arr.resize(size);

    std::vector<char> inBytes;
    for (auto& s : arr)
    {
        file.read((char*)&size, sizeof(u32));
        inBytes.resize(size + 1);
        file.read((char*)inBytes.data(), size + 1);

        s = std::string(inBytes.data());
    }
}

void SaveMap(std::ofstream& file, const std::unordered_map<u32, u32>& map)
{
    std::vector<u32> ms;
    ms.reserve(map.size() * 2);

    for (const auto& m : map)
    {
        ms.push_back(m.first);
        ms.push_back(m.second);
    }

    const u32 size = static_cast<u32>(ms.size());

    file.write((char*)&size, sizeof(size));
    file.write((char*)ms.data(), sizeof(u32) * ms.size());
}

void LoadMap(std::ifstream& file, std::unordered_map<u32, u32>& map)
{
    u32 size = 0;
    file.read((char*)&size, sizeof(size));

    std::vector<u32> ms(size);
    file.read((char*)ms.data(), sizeof(u32) * size);

    for (auto i = 0; i < (size / 2); i++)
        map[ms[i * 2]] = ms[(i * 2) + 1];
}

} // namespace Nerine
