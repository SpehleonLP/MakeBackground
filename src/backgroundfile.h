#ifndef BACKGROUNDFILE_H
#define BACKGROUNDFILE_H
#include "backgroundlayer.h"
#include "filebase.h"
#include <glm/gtc/type_precision.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>

class PngFile;
class DDSFile;

#define VERSION 4

class BackgroundFile : public FileBase
{
public:
	enum
	{
		MAX_MIP = 3
	};

	struct TileInfo
	{
		void setFlag(int x, int y, bool value)
		{
			int index = 1 << (y*4 + x);
			flags &= ~index;
			flags |= value? index : 0;
		}

		uint16_t flags{};
	};

	BackgroundFile(std::string && name);

	void CreateTileDimensions(AlphaFile & alpha_file);

	void Deinterleave();
	void Compress();
	void Append(int type, std::vector<uint8_t> && stuff)
	{
		if(stuff.size())
		{
			if((uint32_t)stuff.size() < stuff.size())
				throw std::runtime_error("datablock too large");

			m_datablocks.push_back({type, std::move(stuff)});
		}
	}


	void WriteOut();
	void WriteFile(FILE * fp);

	uint8_t tiles_x{0};
	uint8_t tiles_y{0};

	int width() const { return (int)tiles_x*256; }
	int height() const { return (int)tiles_y*256; }

	int length() const { return tiles_x * tiles_y; }

	std::unique_ptr<TileInfo[]> tile_info;

	std::unique_ptr<std::vector<uint8_t>[]> encoded[MAX_MIP];

	struct DataBlock
	{
		int32_t				 typeId{0};
		std::vector<uint8_t> buffer;
	};

	std::vector<DataBlock>		m_datablocks;

	BackgroundLayer<DXT1_Block> base_color; //dxt1
	BackgroundLayer<DepthBlock> depth; //uint16_t
	BackgroundLayer<BC5_Block>  normal;  //bc5
	BackgroundLayer<BC4_Block>  occlusion;  //bc4
	BackgroundLayer<BC5_Block>  roughness; //bc5
	bool						unlit{false};

private:
	TileInfo GetDimensions(AlphaFile & alpha_file, int i);
	bool IsSubtileOpaque(AlphaFile & alpha_file, int x, int y, int i);

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
