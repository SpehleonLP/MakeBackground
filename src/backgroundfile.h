#ifndef BACKGROUNDFILE_H
#define BACKGROUNDFILE_H
#include "backgroundlayer.h"
#include "filebase.h"
#include <memory>
#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>

class PngFile;
class DDSFile;

#define VERSION 3

class BackgroundFile : public FileBase
{
public:
	enum
	{
		MAX_MIP = 3
	};

	struct TileInfo
	{
		uint8_t  x0{0};
		uint8_t  y0{0};
		uint8_t  x1{16};
		uint8_t  y1{16};
	};

	BackgroundFile(std::string && name);

	int CreateTileDimensions();
	void ApplyTileDimensions();

	void Deinterleave();
	void Compress();


	void WriteOut();
	void WriteFile(FILE * fp);

	uint8_t tiles_x{0};
	uint8_t tiles_y{0};

	int length() const { return tiles_x * tiles_y; }

	std::unique_ptr<TileInfo[]> tile_info;

	std::unique_ptr<std::vector<uint8_t>[]> encoded[3];

	BackgroundLayer<DXT1_Block> base_color; //dxt1
	BackgroundLayer<BC5_Block>  depth; //uint16_t
	BackgroundLayer<BC5_Block>  normal;  //dxt5
	BackgroundLayer<DXT5_Block> roughness; //dxt5

private:
	template<typename T>
	void SetTileCount(BackgroundLayer<T> & it)
	{
		if(it.tiles_x == 0 && it.tiles_y == 0)
			return;

		if(tiles_x == 0 && tiles_y == 0)
		{
			tiles_x = it.tiles_x;
			tiles_y = it.tiles_y;
		}
		else if(tiles_x != it.tiles_x || tiles_y != it.tiles_y)
		{
			throw std::runtime_error("inconsistent tile counts");
		}
	}

};

#endif // BACKGROUNDFILE_H
