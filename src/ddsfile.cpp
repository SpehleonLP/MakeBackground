#include "backgroundexception.h"
#include "ddsfile.h"
#include "png_file.h"
#include "alphafile.h"
#include "squish.h"
#include "alpha.h"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <fstream>
#include <sstream>

DDSFile::DDSFile(const char * path) :
	FileBase(path)
{
}

void CopyChannelToG(uint8_t * px, PngFile & file, int x, int y, int channel)
{
	for(int i = 0; i < 16; ++i)
	{
		int x1 = x + (i % 4);
		int y1 = y + (i / 4);

		px[i*4+1] = file.row_pointers[y1][x1*file.channels + channel];
	}
}

void CopyRGBA(uint8_t * px, PngFile & file, int x, int y)
{
	if(file.channels == 4)
	{
		memcpy(&px[00], &file.row_pointers[y+0][x * file.channels], 16);
		memcpy(&px[16], &file.row_pointers[y+1][x * file.channels], 16);
		memcpy(&px[32], &file.row_pointers[y+2][x * file.channels], 16);
		memcpy(&px[48], &file.row_pointers[y+3][x * file.channels], 16);
	}
	else
	{
		for(int i = 0; i < 16; ++i)
		{
			int x1 = x + (i % 4);
			int y1 = y + (i / 4);

			for(int j = 0; j < file.channels; ++j)
			{
				if(file.bit_depth == 16)
						px[i*4+j] = ((uint16_t**)file.row_pointers)[y1][x1*file.channels + j];
				else if(file.bit_depth == 8)
						px[i*4+j] = ((uint8_t**)file.row_pointers)[y1][x1*file.channels + j];
			}
		}
	}
}

void CopyChannelToA(uint8_t * px, PngFile & file, int x, int y, int channel)
{
	for(int i = 0; i < 16; ++i)
	{
		int x1 = x + (i % 4);
		int y1 = y + (i / 4);

		if(file.bit_depth == 16)
				px[i*4+3] = ((uint16_t**)file.row_pointers)[y1][x1*file.channels + channel];
		else if(file.bit_depth == 8)
				px[i*4+3] = ((uint8_t**)file.row_pointers)[y1][x1*file.channels + channel];
	}
}


uint8_t * Compress(uint8_t * dst, PngFile & file, int stride, AlphaFile & alpha_file, int mip_layer)
{
	if(file.type == bg_Type::Depth)
	{
		assert(file.channels == 1);
		assert(file.bit_depth == 16);

		size_t row_stride = file.width() * sizeof(uint16_t);

		for(int y = 0; y < file.height(); ++y)
		{
			memcpy(&dst[y * row_stride], &file.row_pointers[y+0][0], row_stride);
		}

		return &dst[file.height()*row_stride];
	}


	uint8_t px[64];
	memset(px, 0, sizeof(px));
	for(int i = 0; i < 16; ++i)	px[i*4 + 3] = 0xFF;

//green if RGB/RGBA, otherwise RED
	int MainChannel = (file.channels >= 3? 1 : 0);

	for(int y = 0; y < file.height(); y += 4)
	{
		for(int x = 0; x < file.width(); x += 4)
		{
			const uint16_t alphaMask = alpha_file.GetMask(mip_layer, x, y);

			switch(file.type)
			{
			case bg_Type::Diffuse:
				CopyRGBA(px, file, x, y);
				squish::CompressMasked(px, alphaMask, dst, squish::kDxt1 | squish::kColourIterativeClusterFit | squish::kWeightColourByAlpha);
				break;
			case bg_Type::Depth:
				CopyChannelToA(px, file, x, y, 0);
				squish::CompressAlphaDxt5(px, 0xFFFF, dst);
				CopyChannelToA(px, file, x, y, 1);
				squish::CompressAlphaDxt5(px, alphaMask, dst+sizeof(BC4_Block));
				break;
			case bg_Type::Normals:
				CopyChannelToA(px, file, x, y, 0);
				squish::CompressAlphaDxt5(px, alphaMask, dst);
				CopyChannelToA(px, file, x, y, 1);
				squish::CompressAlphaDxt5(px, alphaMask, dst+sizeof(BC4_Block));
				break;
			case bg_Type::Roughness:
				CopyChannelToA(px, file, x, y, 1);
				squish::CompressAlphaDxt5(px, alphaMask, dst);
				CopyChannelToA(px, file, x, y, 2);
				squish::CompressAlphaDxt5(px, alphaMask, dst+sizeof(BC4_Block));

			//	CopyChannelToG(px, file, x, y, MainChannel);
			//	squish::CompressMasked(px, alphaMask, dst, squish::kDxt1 | squish::kColourMetricUniform);
				break;
			case bg_Type::Occlusion:
				CopyChannelToA(px, file, x, y, MainChannel);
				squish::CompressAlphaDxt5(px, alphaMask, dst);
				break;
			default:
				throw BackgroundException("Failed to compress; unknown map type.");
				break;
			}

			dst += stride;
		}
	}

	return dst;
}



void DDSFile::create(PngFile ** files, bg_Type type, int layers, AlphaFile & alpha)
{
	switch(files[0]->type)
	{
	case bg_Type::Diffuse:   dx10_header.dxgiFormat = DXGI_FORMAT_BC1_UNORM; break;
	case bg_Type::Depth:     dx10_header.dxgiFormat = DXGI_FORMAT_R16_UNORM; break;
	case bg_Type::Normals:   dx10_header.dxgiFormat = DXGI_FORMAT_BC5_UNORM; break;
	case bg_Type::Roughness: dx10_header.dxgiFormat = DXGI_FORMAT_BC5_UNORM; break;
	case bg_Type::Occlusion: dx10_header.dxgiFormat = DXGI_FORMAT_BC4_UNORM; break;
	default:
		throw std::runtime_error("unknown file type");
		break;
	}

//verify all layers are consistent
	for(int i = 0; i < layers; ++i)
	{
		files[i]->ReadHeader();

		if((files[i]->width() << i) != files[0]->width()
		|| (files[i]->height() << i) != files[0]->height())
		{
			throw BackgroundException("Mip layer ", i, " of ", bg_TypeName(type), " map has dimensions which are not exactly half of the previous layer.");
		}

		if((files[i]->width() % (256 >> i)) != 0
		|| (files[i]->height() % (256 >> i)) != 0)
		{
			throw BackgroundException("Mip layer ", i, " of ", bg_TypeName(type), " map must have a width & height which are both multiples of ", (256 >> i), ".");
		}

		if(files[i]->type != type)
		{
			throw BackgroundException("Mip layers of ", bg_TypeName(type), " map do not all have the same map type.");
		}
	}


	header.dwFlags       =  DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_MIPMAPCOUNT | DDSD_PIXELFORMAT;
	header.dwHeight      = files[0]->height();
	header.dwWidth       = files[0]->width();
	header.dwMipMapCount = layers;

	header.ddspf.dwFlags = 0x4;
	header.ddspf.dwFourCC = TYPE_DX10;

	if(dx10_header.dxgiFormat == DXGI_FORMAT_R16_UNORM)
	{
		header.dwFlags |= DDSD_PITCH;
	}
	else
	{
		header.dwFlags |= DDSD_LINEARSIZE;
	}

	const int stride = GetStride();
	header.dwPitchOrLinearSize = (header.dwWidth/4)*stride;

	if(header.dwFlags & DDSD_LINEARSIZE)
		header.dwPitchOrLinearSize *= (header.dwHeight/4);

	size_t bytes = 0;
	size_t bytes_per_layer = (dx10_header.dxgiFormat == DXGI_FORMAT_R16_UNORM)?
		header.dwHeight * header.dwWidth * sizeof(uint16_t)
		: (header.dwWidth/4)*(header.dwHeight/4)*stride;

	for(int i = 0; i < layers; ++i)
		bytes += bytes_per_layer >> (2*i);

	data.resize(bytes);

	uint8_t * dst = &data[0];

	for(int i = 0; i < layers; ++i)
	{
		files[i]->Read();
		dst  = Compress(dst, *files[i], stride, alpha, i);
		files[i]->clear();
	}
}

#if 0

void DDSFile::DepthFromImage(PngFile & file)
{
	assert(file.channels == 1 && file.bit_depth == 16);

	header.dwFlags |= 0x800008;
	header.dwHeight = file.height;
	header.dwWidth  = file.width;
	header.dwPitchOrLinearSize = 2 * file.width;

	header.ddspf.dwFlags = 0x04;
	header.ddspf.dwFourCC = TYPE_DX10;
	dx10_header.dxgiFormat = DXGI_FORMAT_D16_UNORM;

	data.resize(file.width*file.height*2);

	for(int y = 0; y < file.height; ++y)
		memcpy(&data[y*file.width*2] , file.row_pointers[y], 2 * file.width);
}

void DDSFile::BC4FromImage(PngFile & file, int channel)
{
	assert(file.width % 4 == 0 && file.height % 4 == 0);

	header.dwFlags |= 0x80000;
	header.dwHeight = file.height;
	header.dwWidth  = file.width;
	header.dwPitchOrLinearSize = (file.width/4)*(file.height/4)*8;

	header.ddspf.dwFlags = 0x04;
	header.ddspf.dwFourCC = TYPE_DX10;
	dx10_header.dxgiFormat = DXGI_FORMAT_BC4_UNORM;

	data.resize((file.width/4)*(file.height/4)*8);

	uint8_t px[64];
	memset(px, 0, sizeof(px));

	for(int y = 0; y < file.height; y += 4)
	{
		for(int x = 0; x < file.width; x += 4)
		{
			for(int i = 0; i < 16; ++i)
			{
				int x1 = x + (i % 4);
				int y1 = y + (i / 4);

				px[i*4+3] = file.row_pointers[y1][x1*file.channels + channel];
			}

			int i = (y/4) * (file.width/4) + (x/4);
			squish::CompressAlphaDxt5(px, 0xFFFF, &data[8*i]);
		}
	}
}

void DDSFile::Dxt1FromChannel(PngFile & file, int channel)
{
	file.Read();

	if(file.doesExist == false)
		return;

	assert(file.width % 4 == 0 && file.height % 4 == 0);

	header.dwFlags |= 0x80000;
	header.dwHeight = file.height;
	header.dwWidth  = file.width;
	header.dwPitchOrLinearSize = (file.width/4)*(file.height/4)*8;

	header.ddspf.dwFlags = 0x04;
	header.ddspf.dwFourCC = TYPE_DXT1;

	data.resize((file.width/4)*(file.height/4)*8);

	uint8_t px[64];
	memset(px, 0, sizeof(px));
	for(int i = 0; i < 16; ++i)
		px[i*4 + 3] = 0xFF;

	for(int y = 0; y < file.height; y += 4)
	{
		for(int x = 0; x < file.width; x += 4)
		{
			for(int i = 0; i < 16; ++i)
			{
				int x1 = x + (i % 4);
				int y1 = y + (i / 4);

				px[i*4+1] = file.row_pointers[y1][x1*file.channels + channel];
			}

			int i = (y/4) * (file.width/4) + (x/4);
			squish::Compress(px, &data[8*i], squish::kDxt1);
		}
	}
}

void DDSFile::Dxt1FromImage(PngFile & file)
{
	if(file.channels == 1)
	{
		Dxt1FromChannel(file, 0);
		return;
	}

	file.Read();

	if(file.doesExist == false)
		return;

	assert(file.width % 4 == 0 && file.height % 4 == 0);

	header.dwFlags |= 0x80000;
	header.dwHeight = file.height;
	header.dwWidth  = file.width;
	header.dwPitchOrLinearSize = (file.width/4)*(file.height/4)*8;

	header.ddspf.dwFlags = 0x04;
	header.ddspf.dwFourCC = TYPE_DXT1;

	data.resize((file.width/4)*(file.height/4)*8);

	uint8_t px[64];

	if(file.channels < 3)
		memset(px, 0, sizeof(px));

	if(file.channels != 4)
	{
		for(int i = 0; i < 16; ++i)
			px[i*4 + 3] = 0xFF;
	}

	for(int y = 0; y < file.height; y += 4)
	{
		for(int x = 0; x < file.width; x += 4)
		{
			if(file.channels == 4)
			{
				memcpy(&px[0], &file.row_pointers[y+0][x * file.channels], 16);
				memcpy(&px[1], &file.row_pointers[y+1][x * file.channels], 16);
				memcpy(&px[2], &file.row_pointers[y+2][x * file.channels], 16);
				memcpy(&px[3], &file.row_pointers[y+3][x * file.channels], 16);
			}
			else
			{
				for(int i = 0; i < 16; ++i)
				{
					int x1 = x + (i % 4);
					int y1 = y + (i / 4);

					for(int j = 0; j < file.channels; ++j)
					{
						px[i*4+j] = file.row_pointers[y1][x1*file.channels + j];
					}
				}
			}

			int mask = 0;
			for(int i = 0; i < 16; ++i)
			{
				mask |= (px[i*4 + 3] != 0) << i;
				px[i*4 + 3] = 0xFF;
			}

			int i = (y/4) * (file.width/4) + (x/4);
			squish::CompressMasked(px, mask, &data[8*i], squish::kDxt1);
		}
	}
}

void DDSFile::CreateMetallicRoughness(DDSFile & metal, DDSFile & rough)
{
	const uint8_t g_bitTable[16][2] = {
	{  0, 0  }, //0 0 0 0
	{ 20, 31 }, //0 0 0 1
	{  0, 24 }, //0 0 1 0
	{  0, 18 }, //0 0 1 1
	{  9, 27 }, //0 1 0 0
	{  8, 31 }, //0 1 0 1
	{  9, 24 }, //0 1 1 0
	{  9, 18 }, //0 1 1 1
	{ 31, 20 }, //1 0 0 0
    { 18, 30 }, //1 0 0 1
    { 31,  8 }, //1 0 1 0
    { 18,  0 }, //1 0 1 1
    { 12, 30 }, //1 1 0 0
    { 18,  0 }, //1 1 0 1
    { 18,  9 }, //1 1 1 0
    { 31, 31 }, //1 1 1 1
	};

	if(rough.doesExist == false && metal.doesExist == false)
			return;

	metal.Read();
	rough.Read();


	if(rough.doesExist == false)
	{
		header      = metal.header;
		dx10_header = metal.dx10_header;
//8 bytes per block, 16 pixels per block...
		data.resize(header.dwWidth * header.dwHeight / 2, 0);

		int no_blocks = header.dwWidth * header.dwHeight / 16;
		uint16_t * blocks = (uint16_t*) &data[0];
		for(int i = 0; i < no_blocks; ++i)
		{
			if(blocks[i] == 0)
				continue;

//max out red channel
			data[8*i + 3] = 0x3F;

			for(int j = 0; j < 16; ++j)
			{
				if((blocks[i] >> j) & 0x01)
				{
					data[8*i+(j/4)+4] |= 1 << (2 * (j % 4));
				}
			}
		}

		metal.clear();
		return;
	}

	header      = rough.header;
	dx10_header = rough.dx10_header;
	data        = rough.data;

	if(metal.doesExist == false)
	{
		rough.clear();
		return;
	}

	int no_blocks = header.dwWidth * header.dwHeight / 16;
	uint16_t * blocks = (uint16_t*) &data[0];
	for(int i = 0; i < no_blocks; ++i)
	{
		if(blocks[i] == 0)
			continue;

		uint8_t c[2][4] = {0};

		for(int j = 0; j < 16; ++j)
			c[(blocks[i] >> j) & 0x01][(data[8*i+(j/4)+4] >> (2*j)) & 0x03] += 1;

		uint8_t mask = 0;
		for(int i = 0; i < 4; ++i)
			mask |= (c[0][i] <= c[1][i]);

		data[8*i+1] |= g_bitTable[mask][0];
		data[8*i+3] |= g_bitTable[mask][1];
	}

	rough.clear();
	metal.clear();
}


void DDSFile::BitsFromImage(PngFile & file, int channel)
{
	file.Read();

	if(file.doesExist == false)
		return;

	assert(file.width % 4 == 0 && file.height % 4 == 0);

	header.dwFlags |= 0x80000;
	header.dwHeight = file.height;
	header.dwWidth  = file.width;
	header.dwPitchOrLinearSize = (file.width/4)*(file.height/4)*8;

	header.ddspf.dwFlags = 0x04;
	header.ddspf.dwFourCC = TYPE_BITS;

	data.resize((file.width/4)*(file.height/4)*2);

	uint8_t px[64];
	memset(px, 0, sizeof(px));
	for(int i = 0; i < 16; ++i)
		px[i*4 + 3] = 0xFF;

	for(int y = 0; y < file.height; y += 4)
	{
		for(int x = 0; x < file.width; x += 4)
		{
			uint16_t value = 0;

			for(int i = 0; i < 16; ++i)
			{
				int x1 = x + (i % 4);
				int y1 = y + (i / 4);

				value |= (file.row_pointers[y1][x1*file.channels + channel] >= 128) << 1;
			}

			int i = (y/4) * (file.width/4) + (x/4);
			((uint16_t*) &data[0])[i] = value;
		}
	}
}
#endif

void DDSFile::Read()
{
	if(data.size() || doesExist() == false)
		return;

	std::ifstream file(path, std::ios::binary);

	if (!file.is_open())
		throw std::system_error(std::make_error_code(std::errc::no_such_file_or_directory), path);

	uint32_t file_type;
	file.read(reinterpret_cast<char *>(&file_type), 4);

	if(file_type != TYPE_DDS)
		throw BackgroundException(path + ": not a valid DDS file");

	file.read(reinterpret_cast<char *>(&header), sizeof(header));

	if(header.ddspf.dwFourCC == TYPE_DX10)
		file.read(reinterpret_cast<char *>(&dx10_header), sizeof(dx10_header));

	auto pos = file.tellg();
	file.seekg(0, std::ios_base::end);
	auto length = file.tellg();
	file.seekg(pos);

	data.resize(length - pos);
	file.read(reinterpret_cast<char *>(&data[0]), data.size());
}

void DDSFile::Write()
{
	if(data.empty())
		return;

	std::ofstream file(path, std::ios::binary);

	if (!file.good())
		throw std::system_error(std::make_error_code(std::errc::io_error), path);

	uint32_t file_type = TYPE_DDS;
	file.write(reinterpret_cast<char const *>(&file_type), 4);
	file.write(reinterpret_cast<char const *>(&header), sizeof(header));

	if(header.ddspf.dwFourCC == TYPE_DX10)
		file.write(reinterpret_cast<char const *>(&dx10_header), sizeof(dx10_header));

	file.write(reinterpret_cast<char const *>(&data[0]), data.size());
}

uint32_t DDSFile::GetCompressionId() const
{
	if(header.ddspf.dwFourCC == TYPE_DX10)
		return dx10_header.dxgiFormat;

	if(header.ddspf.dwFourCC == TYPE_DXT1)	return DXGI_FORMAT_BC1_UNORM;
	if(header.ddspf.dwFourCC == TYPE_DXT2)	return DXGI_FORMAT_BC2_UNORM;
	if(header.ddspf.dwFourCC == TYPE_DXT3)	return DXGI_FORMAT_BC2_UNORM;
	if(header.ddspf.dwFourCC == TYPE_DXT4)	return DXGI_FORMAT_BC3_UNORM;
	if(header.ddspf.dwFourCC == TYPE_DXT5)	return DXGI_FORMAT_BC3_UNORM;

	return DXGI_FORMAT_UNKNOWN;
}

int  DDSFile::GetBcId() const
{
	switch(GetCompressionId())
	{
	case DXGI_FORMAT_BC1_TYPELESS: return 1;
	case DXGI_FORMAT_BC1_UNORM: return 1;
	case DXGI_FORMAT_BC1_UNORM_SRGB: return 1;
	case DXGI_FORMAT_BC2_TYPELESS: return 2;
	case DXGI_FORMAT_BC2_UNORM: return 2;
	case DXGI_FORMAT_BC2_UNORM_SRGB: return 2;
	case DXGI_FORMAT_BC3_TYPELESS: return 3;
	case DXGI_FORMAT_BC3_UNORM: return 3;
	case DXGI_FORMAT_BC3_UNORM_SRGB: return 3;
	case DXGI_FORMAT_BC4_TYPELESS: return 4;
	case DXGI_FORMAT_BC4_UNORM: return 4;
	case DXGI_FORMAT_BC4_SNORM: return 4;
	case DXGI_FORMAT_BC5_TYPELESS: return 5;
	case DXGI_FORMAT_BC5_UNORM: return 5;
	case DXGI_FORMAT_BC5_SNORM: return 5;
	case DXGI_FORMAT_BC6H_TYPELESS: return 6;
	case DXGI_FORMAT_BC6H_UF16: return 6;
	case DXGI_FORMAT_BC6H_SF16: return 6;
	case DXGI_FORMAT_BC7_TYPELESS: return 7;
	case DXGI_FORMAT_BC7_UNORM: return 7;
	case DXGI_FORMAT_BC7_UNORM_SRGB: return 7;
	case DXGI_FORMAT_D16_UNORM: return 8;
	case DXGI_FORMAT_R16_UNORM: return 8;
	default:
		break;
	}

	return 0;
}

int DDSFile::GetStride() const
{
	const static uint8_t stride[9] = { 1, sizeof(DXT1_Block), sizeof(DXT5_Block), sizeof(DXT5_Block), sizeof(BC4_Block), sizeof(BC5_Block), sizeof(BC4_Block), sizeof(BC5_Block), 8 };
	return stride[GetBcId()];
}
