#ifndef LINEARIMAGE_H
#define LINEARIMAGE_H
#include "bg_type.h"
#include "png_file.h"
#include <glm/vec2.hpp>
#include <memory>

class PngFile;

class LinearImage
{
public:
	LinearImage() = default;
	~LinearImage() = default;

	void toLinear();
	void fromLinear();

	int width() const { return size.x; }
	int height() const { return size.y; }

	glm::ivec2 size;
	uint8_t channels{1};
	uint8_t bit_depth{8};

	bg_Type type{};
	PngFile::ColorType color_type{};

	std::unique_ptr<float[]> data;

	void toPng(PngFile & dst) const;
	void fromPng(PngFile & src);

	void LinearDownscale(const uint32_t * platform_mask, const std::array<float, 4> & kernel);

private:
	template<typename T>
	void copyDataTo(PngFile & dst, float limit) const;

	template<typename T>
	void copyDataFrom(PngFile const& dst, float limit) const;
};

template<typename T>
inline void LinearImage::copyDataTo(PngFile & png, float limit) const
{
	for(int y = 0; y < height(); ++y)
	{
		T          * dst = (T*)(png.row_pointers[y]);
		float const* src =      &data[y*width()*channels];

		for(int x = 0; x < width()*channels; ++x)
		{
			dst[x] = (T) std::max(0, std::min<int>(src[x] * limit, (T)~0u));
		}
	}
}

template<typename T>
inline void LinearImage::copyDataFrom(const PngFile & png, float limit) const
{
	for(int y = 0; y < height(); ++y)
	{
		T       * src = (T*)(png.row_pointers[y]);
		float   * dst = &data[y*width()*channels];

		for(int x = 0; x < width()*channels; ++x)
		{
			dst[x] = std::max(0.f, std::min(src[x] / limit, 1.f));
		}
	}
}
#endif // LINEARIMAGE_H
