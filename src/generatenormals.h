#ifndef GENERATENORMALS_H
#define GENERATENORMALS_H
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <memory>

class PngFile;
class DepthFile;

void GenerateNormals(PngFile & out, DepthFile & in);
void GradientFromVariable(glm::vec2 * gradient,  const float * distance, uint32_t width, uint32_t height);
void BlurGradient(std::unique_ptr<glm::vec2[]> & gradient, const float * depth, uint32_t width, uint32_t height);
void PngFromGradientField(PngFile & r, glm::vec2 * gradient, glm::ivec2 size);

#endif // GENERATENORMALS_H
