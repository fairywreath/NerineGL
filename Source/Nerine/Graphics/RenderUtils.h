#pragma once

#include <RenderDescription/BoundingBox.h>

#include <Core/Types.h>

#include <vector>

namespace Nerine
{

enum class BitmapType
{
    Texture2D,
    Cube
};

enum class BitmapFormat
{
    UnsignedByte,
    Float,
};

struct Bitmap
{
    Bitmap() = default;
    Bitmap(int w, int h, int comp, BitmapFormat fmt)
        : w_(w), h_(h), comp_(comp), fmt_(fmt), data_(w * h * comp * GetBytesPerComponent(fmt))
    {
        InitGetSetFuncs();
    }
    Bitmap(int w, int h, int d, int comp, BitmapFormat fmt)
        : w_(w), h_(h), d_(d), comp_(comp), fmt_(fmt),
          data_(w * h * d * comp * GetBytesPerComponent(fmt))
    {
        InitGetSetFuncs();
    }
    Bitmap(int w, int h, int comp, BitmapFormat fmt, const void* ptr)
        : w_(w), h_(h), comp_(comp), fmt_(fmt), data_(w * h * comp * GetBytesPerComponent(fmt))
    {
        InitGetSetFuncs();
        memcpy(data_.data(), ptr, data_.size());
    }
    int w_ = 0;
    int h_ = 0;
    int d_ = 1;
    int comp_ = 3;
    BitmapFormat fmt_ = BitmapFormat::UnsignedByte;
    BitmapType type_ = BitmapType::Texture2D;
    std::vector<u8> data_;

    static int GetBytesPerComponent(BitmapFormat fmt)
    {
        if (fmt == BitmapFormat::UnsignedByte)
            return 1;
        if (fmt == BitmapFormat::Float)
            return 4;
        return 0;
    }

    void SetPixel(int x, int y, const vec4& c)
    {
        (*this.*SetPixelFunc)(x, y, c);
    }
    vec4 GetPixel(int x, int y) const
    {
        return ((*this.*GetPixelFunc)(x, y));
    }

private:
    using SetPixel_t = void (Bitmap::*)(int, int, const vec4&);
    using GetPixel_t = vec4 (Bitmap::*)(int, int) const;
    SetPixel_t SetPixelFunc = &Bitmap::SetPixelUnsignedByte;
    GetPixel_t GetPixelFunc = &Bitmap::GetPixelUnsignedByte;

    void InitGetSetFuncs()
    {
        switch (fmt_)
        {
        case BitmapFormat::UnsignedByte:
            SetPixelFunc = &Bitmap::SetPixelUnsignedByte;
            GetPixelFunc = &Bitmap::GetPixelUnsignedByte;
            break;
        case BitmapFormat::Float:
            SetPixelFunc = &Bitmap::SetPixelFloat;
            GetPixelFunc = &Bitmap::GetPixelFloat;
            break;
        }
    }

    void SetPixelFloat(int x, int y, const vec4& c)
    {
        const int ofs = comp_ * (y * w_ + x);
        float* data = reinterpret_cast<float*>(data_.data());
        if (comp_ > 0)
            data[ofs + 0] = c.x;
        if (comp_ > 1)
            data[ofs + 1] = c.y;
        if (comp_ > 2)
            data[ofs + 2] = c.z;
        if (comp_ > 3)
            data[ofs + 3] = c.w;
    }
    vec4 GetPixelFloat(int x, int y) const
    {
        const int ofs = comp_ * (y * w_ + x);
        const float* data = reinterpret_cast<const float*>(data_.data());
        return vec4(comp_ > 0 ? data[ofs + 0] : 0.0f, comp_ > 1 ? data[ofs + 1] : 0.0f,
                    comp_ > 2 ? data[ofs + 2] : 0.0f, comp_ > 3 ? data[ofs + 3] : 0.0f);
    }

    void SetPixelUnsignedByte(int x, int y, const vec4& c)
    {
        const int ofs = comp_ * (y * w_ + x);
        if (comp_ > 0)
            data_[ofs + 0] = u8(c.x * 255.0f);
        if (comp_ > 1)
            data_[ofs + 1] = u8(c.y * 255.0f);
        if (comp_ > 2)
            data_[ofs + 2] = u8(c.z * 255.0f);
        if (comp_ > 3)
            data_[ofs + 3] = u8(c.w * 255.0f);
    }
    vec4 GetPixelUnsignedByte(int x, int y) const
    {
        const int ofs = comp_ * (y * w_ + x);
        return vec4(comp_ > 0 ? float(data_[ofs + 0]) / 255.0f : 0.0f,
                    comp_ > 1 ? float(data_[ofs + 1]) / 255.0f : 0.0f,
                    comp_ > 2 ? float(data_[ofs + 2]) / 255.0f : 0.0f,
                    comp_ > 3 ? float(data_[ofs + 3]) / 255.0f : 0.0f);
    }
};

Bitmap ConvertEquirectangularMapToVerticalCross(const Bitmap& bitmap);
Bitmap ConvertVerticalCrossToCubeMapFaces(const Bitmap& bitmap);

/*
 * Frustum culling calculations.
 */
inline void GetFrustumPlanes(mat4 mvp, vec4* planes)
{
    mvp = glm::transpose(mvp);
    planes[0] = vec4(mvp[3] + mvp[0]); // left
    planes[1] = vec4(mvp[3] - mvp[0]); // right
    planes[2] = vec4(mvp[3] + mvp[1]); // bottom
    planes[3] = vec4(mvp[3] - mvp[1]); // top
    planes[4] = vec4(mvp[3] + mvp[2]); // near
    planes[5] = vec4(mvp[3] - mvp[2]); // far
}

inline void GetFrustumCorners(mat4 mvp, vec4* points)
{
    const vec4 corners[]
        = {vec4(-1, -1, -1, 1), vec4(1, -1, -1, 1), vec4(1, 1, -1, 1), vec4(-1, 1, -1, 1),
           vec4(-1, -1, 1, 1),  vec4(1, -1, 1, 1),  vec4(1, 1, 1, 1),  vec4(-1, 1, 1, 1)};

    const mat4 invMVP = glm::inverse(mvp);

    for (int i = 0; i != 8; i++)
    {
        const vec4 q = invMVP * corners[i];
        points[i] = q / q.w;
    }
}

inline bool IsBoxInFrustum(vec4* frustumPlanes, vec4* frustumCorners, const BoundingBox& box)
{
    using glm::dot;

    for (int i = 0; i < 6; i++)
    {
        int r = 0;
        r += (dot(frustumPlanes[i], vec4(box.min.x, box.min.y, box.min.z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(box.max.x, box.min.y, box.min.z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(box.min.x, box.max.y, box.min.z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(box.max.x, box.max.y, box.min.z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(box.min.x, box.min.y, box.max.z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(box.max.x, box.min.y, box.max.z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(box.min.x, box.max.y, box.max.z, 1.0f)) < 0.0) ? 1 : 0;
        r += (dot(frustumPlanes[i], vec4(box.max.x, box.max.y, box.max.z, 1.0f)) < 0.0) ? 1 : 0;
        if (r == 8)
            return false;
    }

    // Check if frustum is outside or inside box.
    int r = 0;
    r = 0;
    for (int i = 0; i < 8; i++)
        r += ((frustumCorners[i].x > box.max.x) ? 1 : 0);
    if (r == 8)
        return false;
    r = 0;
    for (int i = 0; i < 8; i++)
        r += ((frustumCorners[i].x < box.min.x) ? 1 : 0);
    if (r == 8)
        return false;
    r = 0;
    for (int i = 0; i < 8; i++)
        r += ((frustumCorners[i].y > box.max.y) ? 1 : 0);
    if (r == 8)
        return false;
    r = 0;
    for (int i = 0; i < 8; i++)
        r += ((frustumCorners[i].y < box.min.y) ? 1 : 0);
    if (r == 8)
        return false;
    r = 0;
    for (int i = 0; i < 8; i++)
        r += ((frustumCorners[i].z > box.max.z) ? 1 : 0);
    if (r == 8)
        return false;
    r = 0;
    for (int i = 0; i < 8; i++)
        r += ((frustumCorners[i].z < box.min.z) ? 1 : 0);
    if (r == 8)
        return false;

    return true;
}

/*
 * Obtain one bounding box from all existing boxes.
 */
inline BoundingBox CombineBoxes(const std::vector<BoundingBox>& boxes)
{
    std::vector<vec3> allPoints;
    allPoints.reserve(boxes.size() * 8);

    for (const auto& b : boxes)
    {
        allPoints.emplace_back(b.min.x, b.min.y, b.min.z);
        allPoints.emplace_back(b.min.x, b.min.y, b.max.z);
        allPoints.emplace_back(b.min.x, b.max.y, b.min.z);
        allPoints.emplace_back(b.min.x, b.max.y, b.max.z);

        allPoints.emplace_back(b.max.x, b.min.y, b.min.z);
        allPoints.emplace_back(b.max.x, b.min.y, b.max.z);
        allPoints.emplace_back(b.max.x, b.max.y, b.min.z);
        allPoints.emplace_back(b.max.x, b.max.y, b.max.z);
    }

    return BoundingBox(allPoints.data(), allPoints.size());
}

class FramesPerSecondCounter
{
public:
    explicit FramesPerSecondCounter(float avgInterval = 0.5f) : m_AvgInterval(avgInterval)
    {
        assert(avgInterval > 0.0f);
    }

    bool Tick(float deltaSeconds, bool frameRendered = true)
    {
        if (frameRendered)
            m_NumFrames++;

        m_AccumulatedTime += deltaSeconds;

        if (m_AccumulatedTime > m_AvgInterval)
        {
            m_CurrentFPS = static_cast<float>(m_NumFrames / m_AccumulatedTime);
            m_NumFrames = 0;
            m_AccumulatedTime = 0;
            return true;
        }

        return false;
    }

    inline float GetFPS() const
    {
        return m_CurrentFPS;
    }

private:
    const float m_AvgInterval = 0.5f;
    unsigned int m_NumFrames = 0;
    double m_AccumulatedTime = 0;
    float m_CurrentFPS = 0.0f;
};

} // namespace Nerine