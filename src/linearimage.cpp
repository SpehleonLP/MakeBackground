#include "linearimage.h"
#include "png_file.h"
#include <glm/glm.hpp>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <vector>

void LinearImage::toPng(PngFile & png) const
{
	png.size         = size;
	png.bit_depth    = bit_depth == 16? 16 : 8;
	png.channels     = channels;
	png.bytesPerRow  = channels * width() * (bit_depth == 16? 2 : 1);
	png.color_type   = color_type;

	png.Alloc();

	if(png.bit_depth == 8)
		copyDataTo<uint8_t>(png, 256.f);
	else if(png.bit_depth == 16)
		copyDataTo<uint16_t>(png, 65536.f);
	else
		throw std::runtime_error("unknown bit depth");
}

void LinearImage::fromPng(PngFile & png)
{
	png.Read();

	size       = png.size;
	channels   = png.channels;
	type       = png.type;
	color_type = png.color_type;
	bit_depth  = png.bit_depth;

	data.reset(new float[width()*height()*channels]);

	if(png.bit_depth == 8)
		copyDataFrom<uint8_t>(png, 256.f);
	else if(png.bit_depth == 16)
		copyDataFrom<uint16_t>(png, 65536.f);
	else
		throw std::runtime_error("unknown bit depth");
}

float SUM(float const* array, int length)
{
	float r = 0;
	while(--length >= 0) r += array[length];
	return r;
}

void LinearImage::LinearDownscale(const uint32_t * platform_mask, std::array<float, 4> const& kernel)
{
	glm::ivec2 size0 = size;
	glm::ivec2 size1 = (size >>= 1);

	decltype(data) source(new float[size1.x*size1.y*channels]);
	source.swap(data);

//scale in X direction
	for(int y = 0; y < size1.y; ++y)
	{
		for(int x = 0; x < size1.x; ++x)
		{
			const auto dstMask = platform_mask? platform_mask[y*size1.x+x] : ~0u;
			glm::vec4 & dst = (glm::vec4&)data[(y*size1.x+x)*channels];

			glm::vec4 summation{0.f};
			float denominator = 0.f;

			for(int yd = -2; yd < 2; ++yd)
			{
				for(int xd = -2; xd < 2; ++xd)
				{
					const float coeff = kernel[yd+2] * kernel[xd+2];

					const int index = (yd+2)*4 + xd+2;
					const int x0 = x*2 + xd;
					const int y0 = y*2 + yd;
					
					if(!(0 <= x0 && x0 <= size0.x && 0 <= y0 && y0 <= size0.y))
						continue;
					
					if((dstMask & (1 << index)) == 0)
						continue;

					glm::vec4 & px = (glm::vec4&) source[(y0*size0.x + x0)*channels];

					for(int i = 0; i < channels; ++i)
						summation[i] += px[i] * coeff;

					denominator += coeff;
				}
			}

			if(denominator)
				summation /= denominator;

			for(int i = 0; i < channels; ++i)
				dst[i] = summation[i];
		}
	}
}

void LinearImage::toLinear()
{

	switch(type)
	{
	case bg_Type::Diffuse:
	{
		uint32_t chn = std::max<uint8_t>(3, channels);
		uint32_t N   = height() * width() * channels;

		for(uint32_t i = 0; i < N; i += channels)
		{
			for(uint32_t c = 0; c < chn; ++c)
			{
				data[i + c] = std::pow(data[i + c], 2.2f);
			}
		}
	}
	case bg_Type::Depth:
		break;
	case bg_Type::Normals:
	{
		uint32_t N = height() * width() * channels;

		for(uint32_t i = 0; i < N; ++i)
		{
			data[i] = data[i] * 2.f - 1.f;
		}
	}
		break;
	case bg_Type::Roughness:
		break;
	case bg_Type::Occlusion:
	break;
	default:
		break;
	}
}

void LinearImage::fromLinear()
{
	switch(type)
	{
	case bg_Type::Diffuse:
	{
		uint32_t chn = std::max<uint8_t>(3, channels);
		uint32_t N   = height() * width() * channels;

		for(uint32_t i = 0; i < N; i += channels)
		{
			for(uint32_t c = 0; c < chn; ++c)
			{
				data[i + c] = std::pow(data[i + c], 1.f/2.2f);
			}
		}
	}
	case bg_Type::Depth:
		break;
	case bg_Type::Normals:
	{
		uint32_t N = height() * width() * channels;

		for(uint32_t i = 0; i < N; i += channels)
		{
			glm::vec3 v = glm::vec3(data[i+0], data[i+1], data[i+2]);
			v = glm::normalize(v);
			v = (v + 1.f) / 2.f;

			data[i+0] = v[0];
			data[i+1] = v[1];
			data[i+2] = v[2];
		}
	}
		break;
	case bg_Type::Roughness:
		break;
	case bg_Type::Occlusion:
	break;
	default:
		break;
	}
}
