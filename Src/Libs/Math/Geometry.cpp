/**
 * @file Geometry.cpp
 * Implements class Geometry
 *
 * @author <A href=mailto:juengel@informatik.hu-berlin.de>Matthias Jüngel</A>
 */

#include "Geometry.h"
#include "Math/Eigen.h"
#include "MathBase/Approx.h"
#include "MathBase/BHMath.h"
#include <algorithm>
#include <cstdlib>

float Geometry::angleTo(const Pose2f& from, const Vector2f& to)
{
  const Pose2f relPos = Pose2f(to) - from;
  return std::atan2(relPos.translation.y(), relPos.translation.x());
}

void Geometry::Line::normalizeDirection()
{
  direction.normalize();
}

Geometry::Circle Geometry::getCircle(const Vector2i& point1, const Vector2i& point2, const Vector2i& point3)
{
  const float x1 = static_cast<float>(point1.x());
  const float y1 = static_cast<float>(point1.y());
  const float x2 = static_cast<float>(point2.x());
  const float y2 = static_cast<float>(point2.y());
  const float x3 = static_cast<float>(point3.x());
  const float y3 = static_cast<float>(point3.y());

  Circle circle;
  const float temp = x2 * y1 - x3 * y1 - x1 * y2 + x3 * y2 + x1 * y3 - x2 * y3;

  if(temp == 0)
    circle.radius = 0;
  else
    circle.radius = 0.5f *
                    std::sqrt(((sqr(x1 - x2) + sqr(y1 - y2)) *
                               (sqr(x1 - x3) + sqr(y1 - y3)) *
                               (sqr(x2 - x3) + sqr(y2 - y3))) /
                              sqr(temp));
  if(temp == 0)
    circle.center.x() = 0;
  else
    circle.center.x() = (sqr(x3) * (y1 - y2) +
                         (sqr(x1) + (y1 - y2) * (y1 - y3)) * (y2 - y3) +
                         sqr(x2) * (-y1 + y3)) /
                        (-2.0f * temp);
  if(temp == 0)
    circle.center.y() = 0;
  else
    circle.center.y() = (sqr(x1) * (x2 - x3) +
                         sqr(x2) * x3 +
                         x3 * (-sqr(y1) + sqr(y2)) -
                         x2 * (+sqr(x3) - sqr(y1) + sqr(y3)) +
                         x1 * (-sqr(x2) + sqr(x3) - sqr(y2) + sqr(y3))) /
                        (2.0f * temp);
  return circle;
}

void Geometry::PixeledLine::calculatePixels(const int x1, const int y1, const int x2, const int y2, const int stepSize)
{
  ASSERT(empty()); // only call from constructors
  if(x1 == x2 && y1 == y2)
    emplace_back(x1, y1);
  else
  {
    if(std::abs(x2 - x1) > std::abs(y2 - y1))
    {
      const int sign = sgn(x2 - x1);
      const int numberOfPixels = std::abs(x2 - x1) + 1;
      reserve(numberOfPixels / stepSize);
      for(int x = 0; x < numberOfPixels; x += stepSize)
      {
        const int y = x * (y2 - y1) / (x2 - x1);
        emplace_back(x1 + x * sign, y1 + y * sign);
      }
    }
    else
    {
      const int sign = sgn(y2 - y1);
      const int numberOfPixels = std::abs(y2 - y1) + 1;
      reserve(numberOfPixels / stepSize);
      for(int y = 0; y < numberOfPixels; y += stepSize)
      {
        const int x = y * (x2 - x1) / (y2 - y1);
        emplace_back(x1 + x * sign, y1 + y * sign);
      }
    }
  }
}

bool Geometry::getIntersectionOfLines(const Line& line1, const Line& line2, Vector2f& intersection)
{
  if(line1.direction.y() * line2.direction.x() == line1.direction.x() * line2.direction.y())
    return false;

  intersection.x() =
    line1.base.x() +
    line1.direction.x() *
    (line1.base.y() * line2.direction.x() -
     line2.base.y() * line2.direction.x() +
     (-line1.base.x() + line2.base.x()) * line2.direction.y()) /
    ((-line1.direction.y()) * line2.direction.x() + line1.direction.x() * line2.direction.y());

  intersection.y() =
    line1.base.y() +
    line1.direction.y() *
    (-line1.base.y() * line2.direction.x() +
     line2.base.y() * line2.direction.x() +
     (line1.base.x() - line2.base.x()) * line2.direction.y()) /
    (line1.direction.y() * line2.direction.x() - line1.direction.x() * line2.direction.y());

  return true;
}

int Geometry::getIntersectionOfCircles(const Circle& c0, const Circle& c1, Vector2f& p1, Vector2f& p2)
{
  // dx and dy are the vertical and horizontal distances between
  // the circle centers.
  const float dx = c1.center.x() - c0.center.x();
  const float dy = c1.center.y() - c0.center.y();

  // Determine the straight-line distance between the centers.
  const float d = std::sqrt((dy * dy) + (dx * dx));

  // Check for solvability.
  if(d > (c0.radius + c1.radius)) // no solution. circles do not intersect.
    return 0;
  if(d < std::abs(c0.radius - c1.radius)) //no solution. one circle is contained in the other
    return 0;

  // 'point 2' is the point where the line through the circle
  // intersection points crosses the line between the circle
  // centers.

  // Determine the distance from point 0 to point 2.
  const float a = ((c0.radius * c0.radius) - (c1.radius * c1.radius) + (d * d)) / (2.0f * d);

  // Determine the coordinates of point 2.
  const float x2 = c0.center.x() + (dx * a / d);
  const float y2 = c0.center.y() + (dy * a / d);

  // Determine the distance from point 2 to either of the
  // intersection points.
  const float h = std::sqrt((c0.radius * c0.radius) - (a * a));

  // Now determine the offsets of the intersection points from
  // point 2.
  const float rx = -dy * (h / d);
  const float ry = dx * (h / d);

  // Determine the absolute intersection points.
  p1.x() = x2 + rx;
  p2.x() = x2 - rx;
  p1.y() = y2 + ry;
  p2.y() = y2 - ry;

  return p1 == p2 ? 1 : 2;
}

int Geometry::getIntersectionOfLineAndCircle(const Line& line, const Circle& circle, Vector2f& firstIntersection, Vector2f& secondIntersection)
{
  /* solves the following system of equations:
   *
   * (x - x_m)^2 + (y - y_m)^2 = r^2
   * p + l * v = [x, y]
   *
   * where [x_m, y_m] is the center of the circle,
   * p is line.base and v is line.direction and
   * [x, y] is an intersection point.
   * Solution was found with the help of maple.
   */
  const float divisor = line.direction.squaredNorm();
  const float p = 2 * (line.base.dot(line.direction) - circle.center.dot(line.direction)) / divisor;
  const float q = ((line.base - circle.center).squaredNorm() - sqr(circle.radius)) / divisor;
  const float p_2 = p / 2.0f;
  const float radicand = sqr(p_2) - q;
  if(radicand < 0)
    return 0;
  else
  {
    const float radix = std::sqrt(radicand);
    firstIntersection = line.base + line.direction * (-p_2 + radix);
    secondIntersection = line.base + line.direction * (-p_2 - radix);
    return radicand == 0 ? 1 : 2;
  }
}

bool Geometry::getIntersectionOfRaysFactor(const Line& ray1, const Line& ray2, float& factor)
{
  const float divisor = ray2.direction.x() * ray1.direction.y() - ray1.direction.x() * ray2.direction.y();
  if(divisor == 0)
    return false;
  const float k = (ray2.direction.y() * ray1.base.x() - ray2.direction.y() * ray2.base.x() - ray2.direction.x() * ray1.base.y() + ray2.direction.x() * ray2.base.y()) / divisor;
  const float l = (ray1.direction.y() * ray1.base.x() - ray1.direction.y() * ray2.base.x() - ray1.direction.x() * ray1.base.y() + ray1.direction.x() * ray2.base.y()) / divisor;
  if((k >= 0) && (l >= 0) && (k <= 1) && (l <= 1))
  {
    factor = k;
    return true;
  }
  return false;
}

bool Geometry::getIntersectionOfLineAndConvexPolygon(const std::vector<Vector2f>& polygon, const Line& direction, Vector2f& intersection, const bool isCCW, Line* intersectedLine)
{
  ASSERT(polygon.size() >= 3);
  for(size_t i = 0; i < polygon.size(); ++i)
  {
    Vector2f intersection2D;
    const Vector2f& p1 = polygon[i];
    const Vector2f& p2 = polygon[(i + 1) % polygon.size()];
    const Vector2f dir = p2 - p1;
    const Geometry::Line polygonLine(p1, dir.normalized());
    const bool isLeftP1 = Geometry::isPointLeftOfLine(direction.base, direction.base + direction.direction, p1);
    const bool isLeftP2 = Geometry::isPointLeftOfLine(direction.base, direction.base + direction.direction, p2);
    const bool hasIntersection = isLeftP1 != isLeftP2 && (isCCW ? !isLeftP1 : isLeftP1);
    if(hasIntersection && Geometry::getIntersectionOfLines(direction, polygonLine, intersection2D))
    {
      intersection = intersection2D;
      if(intersectedLine)
        *intersectedLine = polygonLine;
      return true;
    }
  }
  return false;
}

float Geometry::getDistanceToLineSigned(const Line& line, const Vector2f& point)
{
  if(line.direction.x() == 0 && line.direction.y() == 0)
    return distance(point, line.base);

  Vector2f normal;
  normal.x() = line.direction.y();
  normal.y() = -line.direction.x();
  normal.normalize();

  const float c = normal.dot(line.base);

  return normal.dot(point) - c;
}

float Geometry::getDistanceToLine(const Line& line, const Vector2f& point)
{
  return std::abs(getDistanceToLineSigned(line, point));
}

float Geometry::getDistanceToEdge(const Line& line, const Vector2f& point)
{
  if(line.direction.x() == 0 && line.direction.y() == 0)
    return distance(point, line.base);

  const float d = (point - line.base).dot(line.direction) / line.direction.dot(line.direction);

  if(d < 0)
    return distance(point, line.base);
  else if(d > 1.0f)
    return distance(point, line.base + line.direction);
  else
    return getDistanceToLine(line, point);
}

float Geometry::distance(const Vector2f& point1, const Vector2f& point2)
{
  return (point2 - point1).norm();
}

float Geometry::distance(const Vector2i& point1, const Vector2i& point2)
{
  return (point2 - point1).cast<float>().norm();
}

bool Geometry::isPointInsideRectangle(const Vector2f& bottomLeftCorner, const Vector2f& topRightCorner, const Vector2f& point)
{
  return(bottomLeftCorner.x() <= point.x() && point.x() <= topRightCorner.x() &&
         bottomLeftCorner.y() <= point.y() && point.y() <= topRightCorner.y());
}

bool Geometry::isPointInsideRectangle2(const Vector2f& corner1, const Vector2f& corner2, const Vector2f& point)
{
  const Vector2f bottomLeft(std::min(corner1.x(), corner2.x()), std::min(corner1.y(), corner2.y()));
  const Vector2f topRight(std::max(corner1.x(), corner2.x()), std::max(corner1.y(), corner2.y()));
  return isPointInsideRectangle(bottomLeft, topRight, point);
}

bool Geometry::isPointInsideRectangle(const Rect& rect, const Vector2f& point)
{
  const Vector2f bottomLeft(std::min(rect.a.x(), rect.b.x()), std::min(rect.a.y(), rect.b.y()));
  const Vector2f topRight(std::max(rect.a.x(), rect.b.x()), std::max(rect.a.y(), rect.b.y()));
  return isPointInsideRectangle(bottomLeft, topRight, point);
}

bool Geometry::isPointInsideRectangle(const Vector2i& bottomLeftCorner, const Vector2i& topRightCorner, const Vector2i& point)
{
  return(bottomLeftCorner.x() <= point.x() && point.x() <= topRightCorner.x() &&
         bottomLeftCorner.y() <= point.y() && point.y() <= topRightCorner.y());
}

int Geometry::ccw(const Vector2f& p0, const Vector2f& p1, const Vector2f& p2)
{
  const float dx1 = p1.x() - p0.x();
  const float dy1 = p1.y() - p0.y();
  const float dx2 = p2.x() - p0.x();
  const float dy2 = p2.y() - p0.y();
  if(dx1 * dy2 > dy1 * dx2)
    return 1;
  if(dx1 * dy2 < dy1 * dx2)
    return -1;
  // Now (dx1*dy2 == dy1*dx2) must be true:
  if((dx1 * dx2 < 0.0f) || (dy1 * dy2 < 0.0f))
    return -1;
  if((dx1 * dx1 + dy1 * dy1) >= (dx2 * dx2 + dy2 * dy2))
    return 0;
  return 1;
}

bool Geometry::isPointInsideConvexPolygon(const Vector2f polygon[], const int numberOfPoints, const Vector2f& point)
{
  const int orientation = ccw(polygon[0], polygon[1], point);
  if(orientation == 0)
    return true;
  for(int i = 1; i < numberOfPoints; i++)
  {
    const int currentOrientation = ccw(polygon[i], polygon[(i + 1) % numberOfPoints], point);
    if(currentOrientation == 0)
      return true;
    if(currentOrientation != orientation)
      return false;
  }
  return true;
}

bool Geometry::isPointInsidePolygon(const Vector2f& point, const std::vector<Vector2f>& V)
{
  int i, j = static_cast<int>(V.size()) - 1;
  bool oddNodes = false;

  for(i = 0; i < static_cast<int>(V.size()); ++i)
  {
    if((V[i].y() < point.y() && V[j].y() >= point.y()) || (V[j].y() < point.y() && V[i].y() >= point.y()))
    {
      if(V[i].x() + (point.y() - V[i].y()) / (V[j].y() - V[i].y()) * (V[j].x() - V[i].x()) < point.x())
      {
        oddNodes = !oddNodes;
      }
    }
    j = i;
  }
  return oddNodes;
}

bool Geometry::isPointInsidePolygon(const Vector3f& point, const std::vector<Vector3f>& V)
{
  int i, j = static_cast<int>(V.size()) - 1;
  bool oddNodes = false;

  for(i = 0; i < static_cast<int>(V.size()); ++i)
  {
    if((V[i].y() < point.y() && V[j].y() >= point.y()) || (V[j].y() < point.y() && V[i].y() >= point.y()))
    {
      if(V[i].x() + (point.y() - V[i].y()) / (V[j].y() - V[i].y()) * (V[j].x() - V[i].x()) < point.x())
      {
        oddNodes = !oddNodes;
      }
    }
    j = i;
  }
  return oddNodes;
}

bool Geometry::checkIntersectionOfLines(const Vector2f& l1p1, const Vector2f& l1p2, const Vector2f& l2p1, const Vector2f& l2p2)
{
  return (((ccw(l1p1, l1p2, l2p1) * ccw(l1p1, l1p2, l2p2)) <= 0) &&
          ((ccw(l2p1, l2p2, l1p1) * ccw(l2p1, l2p2, l1p2)) <= 0));
}

bool Geometry::clipPointInsideRectangle(const Vector2i& bottomLeftCorner, const Vector2i& topRightCorner, Vector2i& point)
{
  bool clipped = false;
  if(point.x() < bottomLeftCorner.x())
  {
    point.x() = bottomLeftCorner.x();
    clipped = true;
  }
  if(point.x() > topRightCorner.x())
  {
    point.x() = topRightCorner.x();
    clipped = true;
  }
  if(point.y() < bottomLeftCorner.y())
  {
    point.y() = bottomLeftCorner.y();
    clipped = true;
  }
  if(point.y() > topRightCorner.y())
  {
    point.y() = topRightCorner.y();
    clipped = true;
  }
  return clipped;
}

bool Geometry::clipPointInsideRectangle(const Vector2i& bottomLeftCorner, const Vector2i& topRightCorner, Vector2f& point)
{
  bool clipped = false;
  if(point.x() < bottomLeftCorner.x())
  {
    point.x() = static_cast<float>(bottomLeftCorner.x());
    clipped = true;
  }
  if(point.x() > topRightCorner.x())
  {
    point.x() = static_cast<float>(topRightCorner.x());
    clipped = true;
  }
  if(point.y() < bottomLeftCorner.y())
  {
    point.y() = static_cast<float>(bottomLeftCorner.y());
    clipped = true;
  }
  if(point.y() > topRightCorner.y())
  {
    point.y() = static_cast<float>(topRightCorner.y());
    clipped = true;
  }
  return clipped;
}

bool Geometry::clipPointToPolygonBorder(const std::vector<Vector2f>& polygon, Vector2f& point)
{
  //compare the distance to all edges and project point to the nearest one
  float min = getDistanceToEdge(Line(polygon[0], polygon[polygon.size() - 1] - polygon[0]), point);
  size_t n = polygon.size() - 1;
  for(size_t j = 0; j < polygon.size() - 1; j++)
  {
    if(getDistanceToEdge(Line(polygon[j], polygon[j + 1] - polygon[j]), point) < min)
    {
      min = getDistanceToEdge(Line(polygon[j], polygon[j + 1] - polygon[j]), point);
      n = j;
    }
  }
  if(min == 0.f)
    return false;

  point = getOrthogonalProjectionOfPointOnEdge(polygon[n], polygon[n != polygon.size() - 1 ? n + 1 : 0] - polygon[n], point);
  return true;
}

bool Geometry::clipPointInsidePolygon(const std::vector<Vector2f>& polygon, Vector2f& point)
{
  //if the point is already in nothing to do
  if(isPointInsidePolygon(point, polygon))
    return false;
  clipPointToPolygonBorder(polygon, point);
  return true;
}

bool Geometry::clipPointInsideConvexPolygon(const std::vector<Vector2f>& polygon, Vector2f& point)
{
  //if the point is already in nothing to do
  if(isPointInsideConvexPolygon(polygon.data(), static_cast<int>(polygon.size()), point))
    return false;
  clipPointToPolygonBorder(polygon, point);
  return true;
}

bool Geometry::circleIntersectsAxisAlignedRectangle(const Vector2f& cp, float r,
                                                    const Vector2f& p1, const Vector2f& p2)
{
  // Compute borders of rectangle, as parameters can be in different coordinate systems,
  // in which the directions of the axes are different:
  float xMin, xMax, yMin, yMax;
  if(p1.x() < p2.x())
  {
    xMin = p1.x();
    xMax = p2.x();
  }
  else
  {
    xMin = p2.x();
    xMax = p1.x();
  }
  if(p1.y() < p2.y())
  {
    yMin = p1.y();
    yMax = p2.y();
  }
  else
  {
    yMin = p2.y();
    yMax = p1.y();
  }

  // If the circle is too far away from the edges of the rectangle ...
  if(cp.x() < xMin - r ||
     cp.x() > xMax + r ||
     cp.y() < yMin - r ||
     cp.y() > yMax + r)
  {
    //... it cannot intersect or be inside of the rectangle.
    return false;
  }
  // The previous check should cover most situations. However, it is not
  // valid close to the corners of the rectangle, where the x/y coordinates of the center of the circle could be
  // closer than r to the rectangle edges whilst the circle does not intersect anyway.
  else
  {
    // Check for distance between circle center and the four corners of the rectangle
    const float rSquare = sqr(r);
    if((cp.x() < xMin && cp.y() < yMin && (cp - Vector2f(xMin, yMin)).squaredNorm() > rSquare) ||
       (cp.x() < xMin && cp.y() > yMax && (cp - Vector2f(xMin, yMax)).squaredNorm() > rSquare) ||
       (cp.x() > xMax && cp.y() < yMin && (cp - Vector2f(xMax, yMin)).squaredNorm() > rSquare) ||
       (cp.x() > xMax && cp.y() > yMax && (cp - Vector2f(xMax, yMax)).squaredNorm() > rSquare))
      return false;
  else
    return true;
  }
}

bool Geometry::getIntersectionPointsOfLineAndRectangle(const Vector2i& bottomLeft, const Vector2i& topRight,
                                                       const Geometry::Line& line, Vector2i& point1, Vector2i& point2)
{
  int foundPoints = 0;
  Vector2f point[2];
  if(line.direction.x() != 0)
  {
    const float y1 = line.base.y() + (bottomLeft.x() - line.base.x()) * line.direction.y() / line.direction.x();
    if((y1 >= bottomLeft.y()) && (y1 <= topRight.y()))
    {
      point[foundPoints].x() = static_cast<float>(bottomLeft.x());
      point[foundPoints++].y() = y1;
    }
    const float y2 = line.base.y() + (topRight.x() - line.base.x()) * line.direction.y() / line.direction.x();
    if((y2 >= bottomLeft.y()) && (y2 <= topRight.y()))
    {
      point[foundPoints].x() = static_cast<float>(topRight.x());
      point[foundPoints++].y() = y2;
    }
  }
  if(line.direction.y() != 0)
  {
    const float x1 = line.base.x() + (bottomLeft.y() - line.base.y()) * line.direction.x() / line.direction.y();
    if((x1 >= bottomLeft.x()) && (x1 <= topRight.x()) && (foundPoints < 2))
    {
      point[foundPoints].x() = x1;
      point[foundPoints].y() = static_cast<float>(bottomLeft.y());
      if((foundPoints == 0) || ((point[0] - point[1]).norm() > 0.1f))
      {
        foundPoints++;
      }
    }
    const float x2 = line.base.x() + (topRight.y() - line.base.y()) * line.direction.x() / line.direction.y();
    if((x2 >= bottomLeft.x()) && (x2 <= topRight.x()) && (foundPoints < 2))
    {
      point[foundPoints].x() = x2;
      point[foundPoints].y() = static_cast<float>(topRight.y());
      if((foundPoints == 0) || ((point[0] - point[1]).norm() > 0.1f))
      {
        foundPoints++;
      }
    }
  }
  switch(foundPoints)
  {
    case 1:
      point1 = point[0].cast<int>();
      point2 = point1;
      foundPoints++;
      return true;
    case 2:
      if((point[1] - point[0]).dot(line.direction) > 0)
      {
        point1 = point[0].cast<int>();
        point2 = point[1].cast<int>();
      }
      else
      {
        point1 = point[1].cast<int>();
        point2 = point[0].cast<int>();
      }
      return true;
    default:
      return false;
  }
}

bool Geometry::getIntersectionPointsOfLineAndRectangle(const Vector2f& bottomLeft, const Vector2f& topRight,
                                                       const Geometry::Line& line, Vector2f& point1, Vector2f& point2)
{
  int foundPoints = 0;
  Vector2f point[2];
  if(line.direction.x() != 0)
  {
    const float y1 = line.base.y() + (bottomLeft.x() - line.base.x()) * line.direction.y() / line.direction.x();
    if((y1 >= bottomLeft.y()) && (y1 <= topRight.y()))
    {
      point[foundPoints].x() = bottomLeft.x();
      point[foundPoints++].y() = y1;
    }
    const float y2 = line.base.y() + (topRight.x() - line.base.x()) * line.direction.y() / line.direction.x();
    if((y2 >= bottomLeft.y()) && (y2 <= topRight.y()))
    {
      point[foundPoints].x() = topRight.x();
      point[foundPoints++].y() = y2;
    }
  }
  if(line.direction.y() != 0)
  {
    const float x1 = line.base.x() + (bottomLeft.y() - line.base.y()) * line.direction.x() / line.direction.y();
    if((x1 >= bottomLeft.x()) && (x1 <= topRight.x()) && (foundPoints < 2))
    {
      point[foundPoints].x() = x1;
      point[foundPoints].y() = bottomLeft.y();
      if((foundPoints == 0) || ((point[0] - point[1]).norm() > 0.1f))
      {
        foundPoints++;
      }
    }
    const float x2 = line.base.x() + (topRight.y() - line.base.y()) * line.direction.x() / line.direction.y();
    if((x2 >= bottomLeft.x()) && (x2 <= topRight.x()) && (foundPoints < 2))
    {
      point[foundPoints].x() = x2;
      point[foundPoints].y() = topRight.y();
      if((foundPoints == 0) || ((point[0] - point[1]).norm() > 0.1f))
        foundPoints++;
    }
  }
  switch(foundPoints)
  {
    case 1:
      point1 = point[0];
      point2 = point1;
      foundPoints++;
      return true;
    case 2:
      if((point[1] - point[0]).dot(line.direction) > 0)
      {
        point1 = point[0];
        point2 = point[1];
      }
      else
      {
        point1 = point[1];
        point2 = point[0];
      }
      return true;
    default:
      return false;
  }
}

bool Geometry::isPointLeftOfLine(const Vector2f& start, const Vector2f& end, const Vector2f& point)
{
  return ((end.x() - start.x()) * (point.y() - start.y()) - (end.y() - start.y()) * (point.x() - start.x())) > 0.f;
}

Vector2f Geometry::getOrthogonalProjectionOfPointOnLine(const Vector2f& base, const Vector2f& dir, const Vector2f& point)
{
  const float l = (point.x() - base.x()) * dir.x() + (point.y() - base.y()) * dir.y();
  return base + (dir * l);
}

Vector2f Geometry::getOrthogonalProjectionOfPointOnLine(const Line& line, const Vector2f& point)
{
  return getOrthogonalProjectionOfPointOnLine(line.base, line.direction.normalized(), point);
}

Vector2f Geometry::getOrthogonalProjectionOfPointOnEdge(const Vector2f& base, const Vector2f& dir, const Vector2f& point)
{
  const Vector2f projection = getOrthogonalProjectionOfPointOnLine(base, dir.normalized(), point);

  const float d = (projection - base).dot(dir) / dir.dot(dir);

  if(d < 0)
    return base;
  else if(d > 1.0f)
    return base + dir;
  else
    return projection;
}

Vector2f Geometry::getOrthogonalProjectionOfPointOnEdge(const Line& line, const Vector2f& point)
{
  return getOrthogonalProjectionOfPointOnEdge(line.base, line.direction, point);
}

bool Geometry::isPointInsideArc(const Vector2f& point, const Vector2f& center, const Rangea& angleRange, const float radius)
{
  const Vector2f pointToArc = point - center;
  return (pointToArc.squaredNorm() <= sqr(radius) && angleRange.isInside(pointToArc.angle()));
}
