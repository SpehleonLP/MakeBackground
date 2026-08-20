#pragma once
#include "bg_type.h"
#include "filebase.h"
#include "dds_header.h"
#include <vector>
#include <cstdint>

enum
{
	TYPE_DDS  = 0x20534444,
	TYPE_DX10 = 0x30315844,
	TYPE_DXT1 = 0x31545844,
	TYPE_DXT2 = 0x32545844,
	TYPE_DXT3 = 0x33545844,
	TYPE_DXT4 = 0x34545844,
	TYPE_DXT5 = 0x35545844,
	TYPE_BITS = 0x53544942,
};

extern "C"
{

struct BC4_Block
{
	uint8_t c[2];
	uint8_t i[6];
};

struct DXT1_Block
{
	uint16_t c[2];
	uint8_t  i[4];
};

struct DXT5_Block
{
	BC4_Block  alpha;
	DXT1_Block color;
};

struct BC5_Block
{
	BC4_Block  R;
	BC4_Block  G;
};

struct DepthBlock
{
	uint16_t depth[16];
};

}

class PngFile;
class AlphaFile;

class DDSFile : public FileBase
{
public:
	DDSFile(const char *);

	DDS_HEADER       header;
	DDS_HEADER_DXT10 dx10_header;

	std::vector<uint8_t>  data;

	int      GetBcId() const;
	uint32_t GetCompressionId() const;

	void Read();
	void Write();
	void clear() { data.clear(); }

	void create(PngFile ** files, bg_Type type, int layers, AlphaFile & alpha);

	void CreateMetallicRoughness(DDSFile & metal, DDSFile & rough);
	void BitsFromImage(PngFile & file, int channel);

	int GetStride() const;
};
