#include "AABB.hpp"
namespace gims
{
AABB::AABB()
    : m_lowerLeftBottom(std::numeric_limits<f32>::max())
    , m_upperRightTop(-std::numeric_limits<f32>::max())
{
}
AABB::AABB(f32v3 const* const positions, ui32 nPositions)
    : m_lowerLeftBottom(std::numeric_limits<f32>::max())
    , m_upperRightTop(-std::numeric_limits<f32>::max())

{
  f32v3 centreOfMass = f32v3(0.0f);
  for (ui32 i = 0; i < nPositions; i++)
  {
    const auto& p     = positions[i];
    m_lowerLeftBottom = glm::min(m_lowerLeftBottom, p);
    m_upperRightTop   = glm::max(m_upperRightTop, p);
    centreOfMass += p;
  }
  m_center = centreOfMass / nPositions;
}
f32m4 AABB::getNormalizationTransformation() const
{
  // 1. Translate center
  const f32m4 translation = glm::translate(f32m4(1.0f), -m_center);

  // 2. Scale to [-0.5,...,0.5]^3
  const f32v3 diagonalDistance = m_upperRightTop - m_lowerLeftBottom;
  const f32   maxDimension     = glm::max(glm::max(diagonalDistance.x, diagonalDistance.y), diagonalDistance.z);
  const f32   scalingFactor    = 1.0f / maxDimension;
  const f32m4 scale            = glm::scale(f32m4(1.0f), f32v3(scalingFactor));

  return translation * scale;
}
AABB AABB::getUnion(const AABB& other) const
{
  (void)other;
  // Assignment 2
  return *this;
}
const f32v3& AABB::getLowerLeftBottom()
{
  return m_lowerLeftBottom;
}
const f32v3& AABB::getUpperRightTop() const
{
  return m_upperRightTop;
}
AABB AABB::getTransformed(f32m4& transformation) const
{
  (void)transformation;
  // Assignment 2
  return *this;
}
} // namespace gims
