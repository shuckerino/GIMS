#include "AABB.hpp"
namespace gims
{
AABB::AABB()
    : m_lowerLeftBottom(std::numeric_limits<f32>::max())
    , m_upperRightTop(-std::numeric_limits<f32>::max())
    , m_center(-1.0f)
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
  // To calculate the union of two bounding boxes, we need to do the following...
  // Make new AABB with min of lower left corners and max of top right corners
  const auto minLowerLeft   = glm::min(this->m_lowerLeftBottom, other.m_lowerLeftBottom);
  const auto maxUpperRight  = glm::min(this->m_upperRightTop, other.m_upperRightTop);
  auto       newAABB        = AABB();
  newAABB.m_lowerLeftBottom = minLowerLeft;
  newAABB.m_upperRightTop   = maxUpperRight;
  return newAABB;
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
  auto transformedAABB = AABB();
  transformedAABB.m_lowerLeftBottom = f32v4(this->m_lowerLeftBottom, 1.0f) * transformation;
  transformedAABB.m_upperRightTop   = f32v4(this->m_upperRightTop, 1.0f) * transformation;
  return transformedAABB;
}
} // namespace gims
