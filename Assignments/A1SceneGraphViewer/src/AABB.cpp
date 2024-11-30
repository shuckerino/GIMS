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
  for (ui32 i = 0; i < nPositions; i++)
  {
    const auto& p     = positions[i];
    m_lowerLeftBottom = glm::min(m_lowerLeftBottom, p);
    m_upperRightTop   = glm::max(m_upperRightTop, p);
  }
}
f32m4 AABB::getNormalizationTransformation() const
{
  // Scale to [-0.5,...,0.5]^3
  const f32v3 diagonalDistance = m_upperRightTop - m_lowerLeftBottom;
  const f32   maxDimension     = glm::max(glm::max(diagonalDistance.x, diagonalDistance.y), diagonalDistance.z);
  const f32   scalingFactor    = 1.0f / maxDimension;
  const f32m4 scale            = glm::scale(f32m4(1.0f), f32v3(scalingFactor));
  (void)scalingFactor;
  //const f32m4 scale            = glm::scale(f32m4(1.0f), f32v3(1.0f)); // debug

  // Translate center
  const f32v3 center           = diagonalDistance / 2;
  const f32m4 translation = glm::translate(f32m4(1.0f), -center);

  return scale * translation;
}
AABB AABB::getUnion(const AABB& other) const
{
  // To calculate the union of two bounding boxes, we need to do the following...
  // Make new AABB with min of lower left corners and max of top right corners
  const auto minLowerLeft   = glm::min(this->m_lowerLeftBottom, other.m_lowerLeftBottom);
  const auto maxUpperRight  = glm::max(this->m_upperRightTop, other.m_upperRightTop);
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
  transformedAABB.m_lowerLeftBottom = f32v3(transformation * f32v4(this->m_lowerLeftBottom, 1.0f));
  transformedAABB.m_upperRightTop   = f32v3(transformation * f32v4(this->m_upperRightTop, 1.0f));
  return transformedAABB;
}
} // namespace gims
