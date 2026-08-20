#include "alphafile.h"
#include "png_file.h"
#include "depthfile.h"
#include <cstring>
#include <fstream>
#include <iomanip>

void AlphaFile::Load(DepthFile & depth)
{
	depth.Load();

	m_size            = depth.size;

	uint32_t offset[4]{};
	offset[0] = (m_size.x/4) * (m_size.y/4);
	offset[1] = offset[0] + offset[0] / 4;
	offset[2] = offset[1] + offset[0] / 16;
	offset[3] = offset[2] + offset[0] / 64;

	m_heap.reset(new uint16_t[offset[3]]);
	m_mask[0] = &m_heap[0];
	m_mask[1] = &m_heap[offset[0]];
	m_mask[2] = &m_heap[offset[1]];
	m_mask[3] = &m_heap[offset[2]];

	m_length = offset[3];

	memset(&m_heap[0], 0x00, sizeof(m_heap[0]) * m_length);

	for(int i = 0; i < 4; ++i)
	{
		auto platform = depth.GetHeight(i);

		auto size = glm::u16vec2(m_size.x >> i, m_size.y >> i);

	//	auto top_row = &platform[size.x*(size.y-1)];
	//	auto bottom_row = &platform[0];

		for(int y = 0; y < size.y; ++y)
		{
			auto src = &platform[size.x*y];
			auto dst = &m_mask[i][(y / 4) * (size.x / 4)];

			assert(i == 3 || dst < &m_mask[i+1][0]);

			for(int x = 0; x < size.x; ++x)
			{
//16 bit so the highest value is just under 255.98
				bool flag = (src[x] < 255);
				dst[x / 4] |= flag << ((y%4)*4 + x%4);

			//	assert(src != top_row	 || flag == false);
			//	assert(src != bottom_row || flag == true);
			}
		}
	}

	assert(m_mask[0] = &m_heap[0]);
	assert(m_mask[1] = &m_heap[offset[0]]);
	assert(m_mask[2] = &m_heap[offset[1]]);
	assert(m_mask[3] = &m_heap[offset[2]]);

//	assert(GetMask(0, m_size.x-1, m_size.y-1) == false);

	return;
}

void AlphaFile::PrintAlpha(const char * filename, int mip)
{
	PngFile png(filename, bg_Type::Depth, 0);

	png.size        = glm::ivec2(m_size) >> mip;
	png.color_type  = PngFile::ColorType::GRAY;
	png.bit_depth   = 8;
	png.channels    = 1;
	png.bytesPerRow = png.size.x;

	png.Alloc();
/*
	for(int y = 0; y < height; ++y)
	{
		uint16_t * ptr = (uint16_t*)(png.row_pointers[y]);

		for(int x = 0; x < width; ++x)
		{
			auto vec    = glm::dvec2(x / (double) width, y / (double) height);
			vec = (vec - .5) * 2.0;

			auto height = std::sqrt(1.0 - (vec.x*vec.x + vec.y*vec.y));
			height = glm::max(0.0, glm::min(height, 1.0));

			ptr[x] = USHRT_MAX * height;
		}
	}*/

	png.Write();
}

Dimensions AlphaFile::GetDimensions(int tile_id) const
{
	if(empty())
		return Dimensions{0, 0, 64, 64};

	const int tiles_x = m_size.x / 256;
	const int width   = m_size.x / 4;

	glm::ivec2 tile_offset(
		(tile_id % tiles_x) * 64,
		(tile_id / tiles_x) * 64
	);

	if(m_mask[0][tile_offset.y*width + tile_offset.x]
	&& m_mask[0][tile_offset.y*width + tile_offset.x]
	&& m_mask[0][(tile_offset.y+63)*width + (tile_offset.x+63)]
	&& m_mask[0][(tile_offset.y+63)*width + (tile_offset.x+63)])
		return Dimensions{0, 0, 64, 64};

	Dimensions d{64, 64, 0, 0};

//	dbg(tile_id == 393);

	for(short y = 0; y < 64; ++y)
	{
		auto row = &m_mask[0][(tile_offset.y+y)*width + tile_offset.x];

		short min_x = SHRT_MAX;
		short max_x = SHRT_MIN;

		for(short x = 0; x < 64; ++x)
		{
			if(!row[x]) continue;

			min_x = x;
			max_x = std::max<short>(x+1, max_x);

			for(x = 63; x >= max_x; --x)
			{
				if(!row[x]) continue;

				max_x = x+1;
				break;
			}

			break;
		}

		if(min_x < max_x)
		{
			d.min_x = std::min(min_x, d.min_x);
			d.max_x = std::max(max_x, d.max_x);

			d.min_y = std::min(y    , d.min_y);
			d.max_y = y+1;
		}
	}

	return d;
}

uint16_t AlphaFile::GetMask(int mipLayer, int x, int y) const
{
	if(empty())
		return 0xFFFF;

	auto layer = &m_mask[mipLayer][0];
	auto row   = &layer[y / 4 * width() / 4 >> mipLayer];
	auto result = row[x/4];

	return result;
}
