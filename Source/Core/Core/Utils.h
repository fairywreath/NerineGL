#pragma once

namespace Nerine
{

template <typename T> inline void MergeVectors(std::vector<T>& v1, const std::vector<T>& v2)
{
    v1.insert(v1.end(), v2.begin(), v2.end());
}

template <class T, class Index = int>
inline void EraseSelected(std::vector<T>& v, const std::vector<Index>& selection)
{
    v.resize(std::distance(
        v.begin(), std::stable_partition(v.begin(), v.end(), [&selection, &v](const T& item) {
            return !std::binary_search(selection.begin(), selection.end(),
                                       static_cast<Index>(static_cast<const T*>(&item) - &v[0]));
        })));
}

} // namespace Nerine