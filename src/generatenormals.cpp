#include "generatenormals.h"
#include "depthfile.h"
#include "png_file.h"
#include "backgroundexception.h"

#define BLUR_NORMALS 1

void GenerateNormals(PngFile & out, DepthFile & in)
{
	in.Load();

	if(in.m_height.empty())
	{
		throw BackgroundException("Generating Normals: Unable to load depth data!");
	}

	std::unique_ptr<glm::vec2[]> accumulator(new glm::vec2[in.size.x * in.size.y]);

	GradientFromVariable(&accumulator[0], in.GetHeight(0), in.size.x, in.size.y);
//	BlurGradient(accumulator, in.GetHeight(0),  in.size.x, in.size.y);
	PngFromGradientField(out, &accumulator[0], in.size);
	out.Write();
}

void GradientFromVariable(glm::vec2 * gradient, const float * distance, uint32_t width, uint32_t height)
{
//	std::unique_ptr<glm::vec3[]> tmp(new glm::vec3[width*height]);
//	std::unique_ptr<glm::vec2[]> diff(new glm::vec2[width*height]);

#define CheckDistance() (std::fabs(distance[i] - distance[j]) < 1)

//set borders
	for(uint32_t y = 0; y < height; ++y)
	{
		uint32_t yB = y - (y != 0);
		uint32_t yE = y + (y != height-1);

		for(uint32_t x = 0; x < width; ++x)
		{
			if(distance[y*width+x] == -1)
			{
				gradient[y*width+x] = glm::vec3(0, 0, 0);
				continue;
			}

			const int i = y*width+ x;
			int j;
			glm::vec2 acc;
#if 0
			j      = y*width+(x-(x != 0      ));
			acc.x  = CheckDistance()? distance[i] : distance[j];
			j      = y*width+(x+(x != width-1));
			acc.x -= CheckDistance()? distance[i] : distance[j];

			j      = yB*width+x;
			acc.y  = CheckDistance()? distance[i] : distance[j];
			j      = yE*width+x;
			acc.y -= CheckDistance()? distance[i] : distance[j];
#else
			j      = y*width+(x-(x != 0      ));
			acc.x  = distance[j];
			j      = y*width+(x+(x != width-1));
			acc.x -= distance[j];
			j      = yB*width+x;
			acc.y  = distance[j];
			j      = yE*width+x;
			acc.y -= distance[j];
#endif
			if(acc.x != 0)
			{
				int break_point = 0;
				++break_point;
			}

			gradient[y*width+x] = -acc;
		}
	}
}

void BlurGradient(std::unique_ptr<glm::vec2[]> & gradient, float const* depth, uint32_t width, uint32_t height)
{
	std::unique_ptr<glm::vec2[]> tmp(new glm::vec2[width*height]);
	glm::vec2 * in = &gradient[0];

	for(uint32_t y = 0; y < height; ++y)
	{
		uint32_t yB = y - (y != 0);
		uint32_t yE = y + (y != height-1);

		for(uint32_t x = 0; x < width; ++x)
		{
			float   p  = depth[y*width+x];
			glm::vec3 v  = glm::vec3(in[y*width+x]*2.f, 4.f);
			glm::vec2 acc(0, 0);
			float weight = 0.f;

			uint32_t xB = x - (x != 0);
			uint32_t xE = x + (x != width-1);

			for(uint32_t y0 = yB; y0 < yE; ++y0)
				for(uint32_t x0 = xB; x0 < xE; ++x0)
				{
					if(std::fabs(p - depth[y0*width+x0]) < 1)
					{
						float w = glm::dot(v, glm::vec3(in[y0*width+x0]*2.f, 4.f));

						acc    += w * in[y0*width+x0];
						weight += std::fabs(w);
					}
				}

			tmp[y*width+x] = acc / weight;
		}
	}

	gradient.swap(tmp);
}

void NormalsFromGradient(glm::vec3 * dst, glm::vec2 const* src, uint32_t width, uint32_t height)
{
	uint32_t N = width*height;

	for(uint32_t i = 0; i < N; ++i)
	{
		glm::vec2 acc(src[i].x, src[i].y);

		dst[i] = glm::normalize(glm::vec3(src[i] * 2.f, 4.f));
	}
}

void PngFromGradientField(PngFile & r, glm::vec2 * gradient, glm::ivec2 size)
{
	r.size     = size;
	r.color_type = PngFile::ColorType::RGB;
	r.bit_depth  = 8;
	r.channels   = 3;
	r.bytesPerRow = 3 * size.x;

	r.Alloc();

	for(int y = 0; y < size.y; ++y)
	{
		uint8_t * dst = (r.row_pointers[y]);
		glm::vec2 * src = &gradient[y*size.x];

		for(int x = 0; x < size.x; ++x)
		{
			glm::vec3 v = glm::normalize(glm::vec3(src[x]*2.f, 4.f));
			v = glm::clamp((v + 1.f) * 128.f, 0.f, 255.f);

			dst[x * 3 + 0] = v.x;
			dst[x * 3 + 1] = v.y;
			dst[x * 3 + 2] = v.z;
		}
	}
}
