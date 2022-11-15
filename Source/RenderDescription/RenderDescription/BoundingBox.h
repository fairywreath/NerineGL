#pragma once

#include <Core/Types.h>

#include <cmath>
#include <vector>

#include <glm/ext.hpp>
#include <glm/glm.hpp>

namespace Nerine
{

/*
 * AABB Boundinb Box.
 */
struct BoundingBox
{
    vec3 min;
    vec3 max;

    BoundingBox() = default;
    BoundingBox(const vec3& min, const vec3& max) : min(glm::min(min, max)), max(glm::max(min, max))
    {
    }
    BoundingBox(const vec3* points, size_t numPoints)
    {
        vec3 vmin(std::numeric_limits<float>::max());
        vec3 vmax(std::numeric_limits<float>::lowest());

        for (size_t i = 0; i != numPoints; i++)
        {
            vmin = glm::min(vmin, points[i]);
            vmax = glm::max(vmax, points[i]);
        }
        min = vmin;
        max = vmax;
    }

    vec3 GetSize() const
    {
        return vec3(max[0] - min[0], max[1] - min[1], max[2] - min[2]);
    }

    vec3 GetCenter() const
    {
        return 0.5f * vec3(max[0] + min[0], max[1] + min[1], max[2] + min[2]);
    }

    void Transform(const glm::mat4& t)
    {
        vec3 corners[] = {
            vec3(min.x, min.y, min.z), vec3(min.x, max.y, min.z), vec3(min.x, min.y, max.z),
            vec3(min.x, max.y, max.z), vec3(max.x, min.y, min.z), vec3(max.x, max.y, min.z),
            vec3(max.x, min.y, max.z), vec3(max.x, max.y, max.z),
        };
        for (auto& v : corners)
            v = vec3(t * vec4(v, 1.0f));
        *this = BoundingBox(corners, 8);
    }

    BoundingBox GetTransformed(const glm::mat4& t) const
    {
        BoundingBox b = *this;
        b.Transform(t);
        return b;
    }

    void CombinePoint(const vec3& p)
    {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }
};

} // namespace Nerine
