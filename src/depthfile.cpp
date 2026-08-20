#include "depthfile.h"
#include "backgroundexception.h"
#include "png_file.h"
#include "blurheightmap.h"
#include "blur_config.h"
#include <cstring>
#include <algorithm>
#include <iostream>



DepthFile::DepthFile()
{
}

void DepthFile::clear()
{
	m_height.clear();
	platform_mask.reset();
	platformMaskMask &= 0xF0;
}

size_t DepthFile::GetOffset(int mip) const
{
	return (mip > 0) * (size.x * size.y)
		 + (mip > 1) * (size.x * size.y) / 4
		 + (mip > 2) * (size.x * size.y) / 16
	     + (mip > 3) * (size.x * size.y) / 64;
}

float const* DepthFile::GetHeight(int mip)
{
	if(m_height.empty())
		Load();

	platformMaskMask |= 0x10;
	if(platformMaskMask & (0x10 << mip))
		return &m_height[GetOffset(mip)];

	platformMaskMask |= (0x10 << mip);

	float      * dst = &m_height[GetOffset(mip)];
	float const* src = GetHeight(mip-1);

	size_t width  = size.x >> mip;
	size_t height = size.y >> mip;

	std::unique_ptr<float[]> temp(new float[size.x*size.y]);

//copy into temp;
	for(uint32_t y = 0; y < height; ++y)
	{
		for(uint32_t x = 0; x < width; ++x)
		{
			temp[(y*width+x)*4+0] = src[(y*2+0)*width*2 + x*2];
			temp[(y*width+x)*4+1] = src[(y*2+0)*width*2 + x*2+1];
		}

		for(uint32_t x = 0; x < width; ++x)
		{
			temp[(y*width+x)*4+2] = src[(y*2+1)*width*2 + x*2];
			temp[(y*width+x)*4+3] = src[(y*2+1)*width*2 + x*2+1];
		}
	}

	for(uint32_t y = 0; y < height; ++y)
	{
		for(uint32_t x = 0; x < width; ++x)
		{
			auto ptr = &temp[(y*width+x)*4];
			std::sort(ptr, ptr+4);
			dst[y*width+x] = ptr[2];
		}
	}

	return dst;
}

void DepthFile::CopyPlatform(PngFile & file, int mip)
{
	auto depth = GetHeight(mip);

	file.size = size >> mip;
	file.mip = mip;

	file.type = bg_Type::Depth;

	file.bit_depth  = 16;
	file.channels = 1;

	file.bytesPerRow = sizeof(uint16_t) * file.channels * file.size.x;
	file.color_type = PngFile::ColorType::GRAY;

	file.Alloc();

	for(int y = 0; y < file.height(); ++y)
	{
		uint16_t * dst = (uint16_t*)(file.row_pointers[y]);
		float const* src = &depth[y*file.width()];
		auto multiple = 65536.f / MAX_DEPTH;

		for(int x = 0; x < file.width()*file.channels; ++x)
		{
			dst[x] = (uint16_t) std::max(0, std::min<int>(src[x] * multiple, 65535));
			//dst[x] = (dst[x] << 8) | (dst[x] >> 8);
		}
	}
}

//mip is the source of the downscale
const uint32_t * DepthFile::GetPlatformMask(int mip)
{
	if(doesExist() == false)
		return nullptr;

	assert(mip < 3);

//used to be divided by 4 because RGBA
	size_t start = GetOffset(mip);

	if(platformMaskMask & (1 << mip))
		return &platform_mask[start];

	platformMaskMask |= (1 << mip);

	if(platform_mask == nullptr)
	{
		platform_mask.reset(new uint32_t[GetOffset(4)]);
		memset(&platform_mask[0], 0x00, GetOffset(4) * sizeof(platform_mask[0]));
	}

	uint32_t * mask     = &platform_mask[start];
	const float * mip_0 = GetHeight(mip);
	const float * mip_1 = GetHeight(mip+1);

	const glm::ivec2 size0  = size >> (mip);
	const glm::ivec2 size1  = size >> (mip+1);

	for(int y = 0; y < size1.y; ++y)
	{
		for(int x = 0; x < size1.x; ++x)
		{
			const auto dstCmp = mip_1[y*size1.x+x];
			uint32_t & targ   = mask[y*size1.x+x];

			targ = ~0u;
			continue;

			if(dstCmp != 0)
			{
				int break_point{};
				++break_point;
			}

			for(int yd = -2; yd < 2; ++yd)
			{
				const int y0 = y*2 + yd;

				if(!(0 <= y0 && y0 <= size0.y))
					continue;

				for(int xd = -2; xd < 2; ++xd)
				{
					const int index = (yd+2)*4 + xd+2;
					const int x0 = x*2 + xd;

					if(0 <= x0 && x0 <= size0.x)
					{
						auto srcCmp = mip_0[y0*size0.x + x0];
// half a meter
						if(std::fabs(srcCmp - dstCmp) < 8.f)
							targ |= 1 << index;
#ifndef NDEBUG
						else
						{
							int break_point{};
							++break_point;
						}
#endif
					}
				}
			}
		}
	}

	return mask;
}

/*
void DepthFile::WriteDepth(PngFile & r, float limit)
{
	Load();

	r.size       = size;
	r.color_type = PngFile::ColorType::RGB;
	r.bit_depth  = 8;
	r.channels   = 3;
	r.bytesPerRow = 3 * size.x;

	r.Alloc();

	for(int y = 0; y < size.y; ++y)
	{
		uint8_t * dst = r.row_pointers[y];

		for(int x = 0; x < size.x; ++x)
			dst[x * 3 + 0] = platform[y*size.x + x];
	}

	const float mul = 1.f / limit;

	for(int y = 0; y < size.y; ++y)
	{
		uint8_t * dst = r.row_pointers[y];

		for(int x = 0; x < size.x; ++x)
		{
			float v = height[y*size.x + x] - dst[x*3 + 0];

			dst[x * 3 + 1] = std::min(255u, (uint32_t) (v * mul));
			dst[x * 3 + 2] = 0;
		}
	}

	r.Write();
}*/
/*
void DepthFile::CopyPlatform(PngFile & file)
{
	if(file.row_pointers == nullptr)
		file.Read();

	if(file.row_pointers == nullptr)
		throw BackgroundException("Unable to load image");

	if(size.x != 0 || size.y != 0)
	{
		if(size.x != file.width() || size.y != file.height())
			throw BackgroundException("Platform map size does not match previous depth data");
	}

	size.x = file.width();
	size.y = file.height();

	if((file.width() & 0xFF) != 0
	|| (file.height() & 0xFF) != 0)
	{
		throw BackgroundException("Platform map width/height must be multiples of 256");
	}

	if(file.bit_depth != 8)
	{
		throw BackgroundException("Platform map must have a bit depth of 8");
	}

	if(platform == nullptr)
		platform.reset(new uint32_t[size.x * size.y * 3 / 2]);

//copy in...
	for(int y = 0; y < size.y; ++y)
	{
		auto src = file.row_pointers[y];
		auto dst = &platform[y * size.x];

		for(int x = 0; x < size.x; ++x)
		{
			uint32_t color = 0;
			for(int i = 0; i <= file.channels; ++i)
			{
				color |= src[x*file.channels] << (i*8);
			}

			dst[x] = color;
		}
	}
}*/

bool DepthFile::doesExist() const
{
	if(m_exists == -1)
		m_exists = PngFile("Depth.png", bg_Type::Depth, 0).doesExist();

	return m_exists;
}

void DepthFile::ReadHeader()
{
	if(size.x > 0 || size.y > 0)
		return;

	PngFile png("Depth.png", bg_Type::Depth, 0);

	if(!png.doesExist())
	{
		m_exists = 0;
		throw BackgroundException("Unable to locate depth map");
	}

	png.ReadHeader();
	size = png.size;
	modified = png.modified;
}

PngFile DepthFile::LocateDepth(std::string const& name, bool needFile)
{
	std::string p(name);
	PngFile raw(name, bg_Type::Depth, 0);

	if(raw.ChangePath(p + ".png"))
	{
		raw.ReadHeader();

		if(raw.bit_depth == 16 && raw.channels == 1)
			return raw;
	}

	if(!raw.doesExist())
	{
		if(needFile)
			throw BackgroundException(std::string("Unable to locate ") + p + " map");

		return raw;
	}

	FileBase config(p + "-Config.json");
	PngFile png(p + "-16.gen.png", bg_Type::Depth, 0);

	if(png.moreRecent(raw)
	&& png.moreRecent(config))
		return png;

	throw BackgroundException(std::string("Unable to locate ") + p + " map");

#if 0
	std::cout << "Generating " << name << "-16.gen.png (this can a while)" << std::endl;

	auto stages = ReadConfiguration(config.path);
	BlurHeightMap(png, raw, *this, stages);
	png.Write();
#endif
	return png;
}

void DepthFile::CheckDepth(const char * name)
{
	PngFile png = LocateDepth(name, false);

	if(png.doesExist())
		modified = std::max(png.modified, modified);
}

void DepthFile::AddDepth(const char * name, float multiple, bool needFile)
{
	std::string p(name);

	PngFile png = LocateDepth(p, needFile);

	if(!png.doesExist())
		return;

	png.Read();

	modified = std::max(png.modified, modified);

	if(png.width() != size.x
	|| png.height() != size.y)
	{
		throw BackgroundException(std::string("Dimensions of: '") + p + "' do not match that of the platform map");
	}

	if(png.bit_depth == 16)
	{
		multiple = multiple / (float) 0x0010000;

		for(int y = 0; y < png.height(); ++y)
		{
			uint16_t * src = (uint16_t*) (png.row_pointers[y]);

			for(int x = 0; x < png.width(); ++x)
			{
				auto v = src[x*png.channels + 0];
				//v = (v << 8) | (v >> 8);

				m_height[y*png.width() + x] = (v * multiple);
			}
		}
	}
	else if(png.bit_depth == 8)
	{
		if(png.channels < 3)
		{
			throw BackgroundException(png.path + std::string(": 8 bit '-16' maps must be in RGB color space!"));
		}

		float mul_g = multiple / (float) 0x0010000;
		float mul_b = multiple / (float) 0x0000100;

		for(int y = 0; y < png.height(); ++y)
		{
			uint16_t * src = (uint16_t*) png.row_pointers[y];

			for(int x = 0; x < png.width(); ++x)
			{
				m_height[y*png.width() + x]
					+= src[x*png.channels + 1] * mul_g
					+  src[x*png.channels + 2] * mul_b;
			}
		}
	}
	else
	{
		throw BackgroundException(png.path + std::string(": not a 16 bit image."));
	}
}

void DepthFile::Load()
{
	if(m_height.size())
		return;

//	ReadPlatform();

//--------------------------
// create depth
//--------------------------
	m_height.resize(size.x * size.y * 3 / 2, 0.f);
//	memset(&height[0], 0, sizeof(float) * size.x * size.y * 3 / 2);

//--------------------------
// Get 16 bit depth...
//--------------------------
	float * top_row = &(m_height[0]);
	float * bottom_row = &(m_height[(size.y-1) * size.x]);

	AddDepth("Depth", MAX_DEPTH, true);

//	AddDepth("Detail", 1, false);
}
