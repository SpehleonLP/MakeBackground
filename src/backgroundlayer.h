#ifndef BACKGROUNDLAYER_H
#define BACKGROUNDLAYER_H
#include "backgroundexception.h"
#include <memory>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "ddsfile.h"



void CopyBlock(DXT5_Block * dst, const uint8_t * src, int BC, int block_id);
void CopyBlock(DXT1_Block * dst, const uint8_t * src, int BC, int block_id);
void CopyBlock(BC4_Block * dst, const uint8_t * src, int BC, int block_id);
void CopyBlock(BC5_Block * dst, const uint8_t * src, int BC, int block_id);

class DDSFile;

template<typename BC_ID>
class BackgroundLayer
{
public:
typedef std::vector<BC_ID> TileData;
typedef std::vector<TileData> TileArray;

	BackgroundLayer() = default;

	void SetFile(DDSFile & file)
	{
		file.Read();
		modified = file.modified;

		if(file.data.empty())
			return;

		if(file.header.dwWidth % 256 != 0 || file.header.dwHeight % 256 != 0)
		{
			throw DDSException(file.path + ": image width/height not evenly divisible by 256.");
		}

		if(tiles_x == 0 && tiles_y == 0)
		{
			tiles_x = file.header.dwWidth / 256;
			tiles_y = file.header.dwHeight / 256;
		}
		else if(file.header.dwWidth / 256 != tiles_x || file.header.dwHeight / 256 != tiles_y)
		{
			throw DDSException(file.path + ": image size does not match previous files");
		}

		//const size_t no_tiles   = length();
		const int    BC         = file.GetBcId();
		const int    stride     = BC == 8? 32 : file.GetStride();

		int blocks     = (256 * 256) / 16;

	//	if(BC == 0)
	//		throw DDSException(file.path + ": non-BC1-5 compression type");

		uint8_t * src = &file.data[0];

		uint32_t N = std::min<uint32_t>(file.header.dwMipMapCount, 4);

		for(uint32_t i = 0; i < N; ++i)
		{
			CopyTiles(tiles[i], src, 256 >> i, BC);
			src     += length() * stride * blocks;
			blocks >>= 2;
		}

		file.clear();
	}

	uint32_t tiles_x{0};
	uint32_t tiles_y{0};

	size_t length() const { return tiles_x * tiles_y; }
	bool empty() const { return length() == 0; }

	time_t  modified{0};

	std::vector<std::vector<BC_ID>> tiles[4];
	std::vector<std::vector<BC_ID>> & operator[](int i) { return tiles[i]; }

private:
	void CreateTile(std::vector<BC_ID> & dst, int blocks)
	{
		if(dst.empty())
		{
			BC_ID bc;
			memset(&bc, 0, sizeof(BC_ID));
			dst.resize(blocks, bc);
		}
	}

	void CreateTile(std::unique_ptr<BC_ID[]> & dst, int blocks)
	{
		if(dst == nullptr)
		{
			dst.reset(new BC_ID[blocks]);
			memset(&dst[0], 0, sizeof(BC_ID)*blocks);
		}
	}

	void CopyTiles(TileArray & dst, const uint8_t * src, int tile_size, int BC)
	{
		const int blocks = (tile_size * tile_size) / 16;
		const int tiles  = tiles_x * tiles_y;

		if(dst.empty())
			dst.resize(tiles);

		const int tile_width = tile_size / 4;
		const int row_width  = tile_width * tiles_x;

		for(int i = 0; i < tiles; ++i)
		{
			const int x_offset = (i % tiles_x) * tile_width;
			const int y_offset = (i / tiles_x) * tile_width;

			CreateTile(dst[i], blocks);

			for(int j = 0; j < blocks; ++j)
			{
				const int x = (j % tile_width);
				const int y = (j / tile_width);

				CopyBlock((BC_ID*) &dst[i][j], src, BC, (y_offset + y) * row_width + x_offset + x);
			}
		}
	}

};


template<>
inline void BackgroundLayer<DepthBlock>::CopyTiles(TileArray & dst, const uint8_t * file_data, int tile_size, int )
{
	const uint16_t * src = (uint16_t const*)file_data;
	const int blocks = (tile_size * tile_size) / 16;
	const int tiles  = tiles_x * tiles_y;

	if(dst.empty())
		dst.resize(tiles);

	const int tile_width = tile_size;
	const int row_width  = tile_width * tiles_x;

	for(int i = 0; i < tiles; ++i)
	{
		const int x_offset = (i % tiles_x) * tile_width;
		const int y_offset = (i / tiles_x) * tile_width;

		CreateTile(dst[i], blocks);

		for(int j = 0; j < blocks; ++j)
		{
			const int x = ((j*16) % tile_width);
			const int y = ((j*16) / tile_width);

			int block_id = (y_offset + y) * row_width + x_offset + x;

			memcpy(dst[i][j].depth, &src[block_id], sizeof(DepthBlock));
		}
	}
}
#endif // BACKGROUNDLAYER_H
