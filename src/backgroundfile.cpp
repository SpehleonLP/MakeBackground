#define _CRT_SECURE_NO_WARNINGS
#include "backgroundfile.h"
#include "bg_type.h"
#include "ddsfile.h"
#include "alphafile.h"
#include <cstring>
#include <algorithm>
#include <type_traits>
#include <climits>
#include <cassert>
#include <system_error>
#include <cstdio>
#include <glm/vec2.hpp>

#include "lz4/lib/lz4.h"
#include "lz4/lib/lz4hc.h"

#ifdef _WIN32
#include <intrin.h>
#else
#define __debugbreak() { asm("int3"); }
#endif

#ifndef NDEBUG
#define dbg(x) if(x) __debugbreak()
#endif

const char * bg_FileName(bg_Type it)
{
	const char * bg_Labels[] = { "BaseColor", "Depth", "Normals", "MetallicRoughness", "AmbientOcclusion" };

	return bg_Labels[(int)it];

}

const char * bg_TypeName(bg_Type it)
{
	const char * name[] =
	{
		"BaseColor",
		"Depth",
		"Normals",
		"Roughness",
		"Occlusion",
		"Platform"
	};

	return name[(int)it];
}


bool isTransparent(DXT5_Block & m);
bool isTransparent(DXT1_Block & m);
bool isTransparent(BC4_Block & m);

BackgroundFile::BackgroundFile(std::string && name) :
	FileBase(std::move(name))
{

}

void BackgroundFile::WriteOut()
{
	FILE * fp = fopen(path.c_str(), "wb");

	if(fp == nullptr)
	{
		perror("Failed to open output file: ");
		return;
	}
/*
	uint32_t version = 1;

	uint16_t width = tiles_x * 256;
	uint16_t height = tiles_y * 256;
	uint32_t image_offsets[32];
	memset(image_offsets, 0, 32*sizeof(uint32_t));

	image_offsets[0] = 4 * 34;

	fwrite(&version, sizeof(uint32_t), 1, fp);
	fwrite(&width , sizeof(uint16_t), 1, fp);
	fwrite(&height, sizeof(uint16_t), 1, fp);
	fwrite(image_offsets, sizeof(uint32_t), 32, fp);
*/
	WriteFile(fp);

	for(auto & datablock : m_datablocks)
	{
		uint32_t length = datablock.buffer.size();

		if(datablock.buffer.size() != length)
			throw std::runtime_error("datablock too large");

		fwrite("BLCK", 1, 4, fp);
		fwrite(&datablock.typeId, 1, 4, fp);
		fwrite(&length, 1, 4, fp);
		fwrite(datablock.buffer.data(), 1, length, fp);

		datablock.buffer.clear();
	}

	fclose(fp);
}

void BackgroundFile::WriteFile(FILE * fp)
{
	const char * title = "lbck";
	short version = VERSION | (unlit? 0x8000 : 0);

	size_t offset = ftell(fp);
	uint16_t width = tiles_x * 256;
	uint16_t height = tiles_y * 256;

	fwrite(title, 1, 4, fp);
	fwrite(&version, 2, 1, fp);
	fwrite(&tiles_x, 1, 1, fp);
	fwrite(&tiles_y, 1, 1, fp);
	fwrite(&width , sizeof(uint16_t), 1, fp);
	fwrite(&height, sizeof(uint16_t), 1, fp);

	fwrite(&tile_info[0], sizeof(TileInfo), length(), fp);

	std::vector<uint32_t> mip(MAX_MIP * length() + 1, 0);

	fpos_t position;
	fgetpos(fp, &position);
	fseek(fp, sizeof(uint32_t) * mip.size(), SEEK_CUR);

	for(int i = 0; i < length(); ++i)
	{
		for(int j = 0; j < MAX_MIP; ++j)
		{
			mip[i*MAX_MIP + j] = ftell(fp) - offset;

			if(encoded[j] != nullptr)
			{
				auto & vec = encoded[j][i];

				if(vec.size())
				{
					fwrite(&vec[0], sizeof(uint8_t), vec.size(), fp);
				}
			}
		}
	}

	mip.back() = ftell(fp) - offset;

	fsetpos(fp, &position);
	fwrite(&mip[0], sizeof(mip[0]), mip.size(), fp);

	fseek(fp, 0, SEEK_END);
}

void * deinterleave_primitive(void * r_dst, void * r_src, int blocks, int bytes, int stride)
{
	uint8_t * dst = (uint8_t*) r_dst;
	uint8_t * src = (uint8_t*) r_src;

	for(int j = 0; j < blocks; ++j)
	{
		for(int i = 0; i < bytes; ++i)
		{
			*dst = src[j * stride + i];
			++dst;
		}
	}

	return dst;
}

void * deinterleave_primitive_reps(void * r_dst, void * r_src, int blocks, int bytes)
{
	uint8_t * dst = (uint8_t*) r_dst;
	uint8_t * src = (uint8_t*) r_src;

	for(int j = 0; j < blocks; ++j)
	{
		for(int i = 0; i < bytes; ++i)
		{
			*dst = src[i];
			++dst;
		}
	}

	return dst;
}


#define DEINTERLEAVE 1

void * deinterleave_block(void * dst, DXT1_Block * src, int blocks)
{
#if DEINTERLEAVE
	dst = deinterleave_primitive_reps(dst, &src[0].c[0], blocks, 2);
	dst = deinterleave_primitive_reps(dst, &src[0].c[1], blocks, 2);
	dst = deinterleave_primitive_reps(dst, &src[0].i[0], blocks, 4);
	struct DXT1_Block
	{
		uint16_t c[2];
		uint8_t  i[4];
	};

	return dst;

#else
	auto * i   = (DXT1_Block*) dst;
	auto * end = i + blocks;

	for(; i < end; ++i)
		*i = *src;

	return end;
#endif
}

void * deinterleave_block(void * dst, BC4_Block * src, int blocks)
{
#if DEINTERLEAVE
	dst = deinterleave_primitive_reps(dst, &src[0].c[0], blocks, 1);
	dst = deinterleave_primitive_reps(dst, &src[0].c[1], blocks, 1);
	dst = deinterleave_primitive_reps(dst, &src[0].i[0], blocks, 6);

	return dst;
#else
	auto * i   = (BC4_Block*) dst;
	auto * end = i + blocks;

	for(; i < end; ++i)
		*i = *src;

	return end;
#endif
}

void * deinterleave_block(void * dst, DXT5_Block * src, int blocks)
{
	dst = deinterleave_block(dst, &src[0].alpha, blocks);
	dst = deinterleave_block(dst, &src[0].color, blocks);

	return dst;
}

void * deinterleave_block(void * dst, BC5_Block * src, int blocks)
{
	dst = deinterleave_block(dst, &src[0].R, blocks);
	dst = deinterleave_block(dst, &src[0].G, blocks);

	return dst;
}

void * deinterleave_block(void * dst, DepthBlock * src, int blocks)
{
	DepthBlock * targ = (DepthBlock*)dst;
	for(int i = 0; i < blocks; ++i, ++targ)
		*targ = *src;
	return targ;
}


void * deinterleave(void * dst, DXT1_Block * src, int blocks, int stride = sizeof(DXT1_Block))
{
#if DEINTERLEAVE
	dst = deinterleave_primitive(dst, &src[0].c[0], blocks, 2, stride);
	dst = deinterleave_primitive(dst, &src[0].c[1], blocks, 2, stride);
	dst = deinterleave_primitive(dst, &src[0].i[0], blocks, 4, stride);

	return dst;
#else
	memcpy(dst, &src[0], blocks * sizeof(src[0]));
	return (uint8_t *) dst + (blocks * sizeof(src[0]));
#endif
}


void * deinterleave(void * dst, BC4_Block * src, int blocks, int stride = sizeof(BC4_Block))
{
#if DEINTERLEAVE
	dst = deinterleave_primitive(dst, &src[0].c[0], blocks, 1, stride);
	dst = deinterleave_primitive(dst, &src[0].c[1], blocks, 1, stride);
	dst = deinterleave_primitive(dst, &src[0].i[0], blocks, 6, stride);

	return dst;
#else
	memcpy(dst, &src[0], blocks * sizeof(src[0]));
	return (uint8_t *) dst + (blocks * sizeof(src[0]));
#endif
}

void * deinterleave(void * dst, DXT5_Block * src, int blocks)
{
	dst = deinterleave(dst, &src[0].alpha, blocks, sizeof(DXT5_Block));
	dst = deinterleave(dst, &src[0].color, blocks, sizeof(DXT5_Block));

	return dst;
}

void * deinterleave(void * dst, BC5_Block * src, int blocks)
{
#if DEINTERLEAVE
	dst = deinterleave(dst, &src[0].R, blocks, sizeof(BC5_Block));
	dst = deinterleave(dst, &src[0].G, blocks, sizeof(BC5_Block));
#else
	memcpy(dst, &src[0], blocks * sizeof(src[0]));
	return (uint8_t *) dst + (blocks * sizeof(src[0]));
#endif

	return dst;
}

void * deinterleave(void * dst, DepthBlock * src, int blocks)
{
	int pixels = blocks * sizeof(*src) / sizeof(uint16_t);
	assert(pixels * sizeof(uint16_t) == blocks * sizeof(DepthBlock));

#if DEINTERLEAVE
	uint8_t * dst0 = (uint8_t*)dst;
	uint8_t * src0 = (uint8_t*)src;

	for(int i = 0; i < pixels; ++i)
	{
		dst0[i] = src0[i*2+0];
	}

	for(int i = 0; i < pixels; ++i)
	{
		dst0[i+pixels] = src0[i*2+1];
	}

	int8_t * ptr = (int8_t*)dst;
	for(int i = blocks*2-1; i > 0; --i)
	{
		ptr[i] = ptr[i] - ptr[i-1];
	}

#else
	memcpy(dst, src, blocks * sizeof(*src));
#endif

	return (DepthBlock*)dst + blocks;
}

void BackgroundFile::Deinterleave()
{
	unlit = depth.empty();

	enum {
		BaseColorBlockSize = sizeof(DXT1_Block),
		RoughBlockSize		= sizeof(BC5_Block),
		NormalBlockSize		= sizeof(BC5_Block),
		OcclusionBlockSize  = sizeof(BC4_Block),
		DepthBlockSize      = 2*16,
	};

	enum
	{
		BaseColorOffset   = 0,
		RoughOffset       = BaseColorOffset + BaseColorBlockSize,
		NormalOffset      = RoughOffset     + RoughBlockSize,
		OcclusionOffset   = NormalOffset    + NormalBlockSize,
		DepthOffset       = OcclusionOffset + OcclusionBlockSize,
		DeinterlacedBytes = DepthOffset     + DepthBlockSize,
	};

	DepthBlock default_depth;
	BC5_Block default_normals;

	BC5_Block default_roughness;
	DXT1_Block default_diffuse;

	memset(&default_depth,     0, sizeof(default_depth));
	memset(&default_normals,   0, sizeof(default_normals));
	memset(&default_roughness, 0, sizeof(default_roughness));
	memset(&default_diffuse,   0, sizeof(default_diffuse));

//default diffuse is solid white
	default_diffuse.c[0] = 0xFFFF;

//default roughness is 50% rough dialectric
	default_roughness.R.c[0] = 128;
	default_roughness.G.c[0] = 0;

//default depth is 1 away from sky
	//default_depth.R.c[0] = 1;

//default normals point forward
	default_normals.R.c[0] = 128;
	default_normals.G.c[0] = 128;

	for(int i = 0; i < MAX_MIP; ++i)
	{
		if(encoded[i] == nullptr)
			encoded[i] = std::unique_ptr<std::vector<uint8_t>[]>(new std::vector<uint8_t>[length()]);

		for(int j = 0; j < length(); ++j)
		{
			const int w = 256/4 >> i;
			const int h = 256/4 >> i;

			const size_t blocks = (w*h);
			if(tile_info[j].flags == 0) continue;

			const size_t bytes = depth.empty()? blocks * BaseColorBlockSize : blocks * DeinterlacedBytes;

			encoded[i][j].resize(bytes);
			auto & vec = encoded[i][j];

			if(base_color[i].size() && base_color[i][j].size())
			{
				deinterleave(&vec[blocks*BaseColorOffset], base_color[i][j].data(), blocks);
				base_color[i][j].clear();
			}
			else
			{
				deinterleave_block(&vec[blocks*BaseColorOffset], &default_diffuse, blocks);
			}

//unlit doesn't have these layers
			if(!depth.empty())
			{
				if(roughness[i].size() && roughness[i][j].size())
				{
					deinterleave(&vec[blocks*RoughOffset], roughness[i][j].data(), blocks);
					roughness[i][j].clear();
				}
				else
				{
					deinterleave_block(&vec[blocks*RoughOffset], &default_roughness, blocks);
				}

				if(normal[i].size() && normal[i][j].size())
				{
					deinterleave(&vec[blocks*NormalOffset], normal[i][j].data(), blocks);
					normal[i][j].clear();
				}
				else
				{
					deinterleave_block(&vec[blocks*NormalOffset], &default_normals, blocks);
				}

				if(occlusion[i].size() && occlusion[i][j].size())
				{
					deinterleave(&vec[blocks*OcclusionOffset], occlusion[i][j].data(), blocks);
					occlusion[i][j].clear();
				}
				else
				{
					deinterleave_block(&vec[blocks*OcclusionOffset], &default_normals, blocks);
				}

				if(depth[i].size() && depth[i][j].size())
				{
					deinterleave(&vec[blocks*DepthOffset], depth[i][j].data(), blocks);
					depth[i][j].clear();
				}
				else
				{
					deinterleave_block(&vec[blocks*DepthOffset], &default_depth, blocks);
				}
			}
		}

		base_color[i].clear();
		roughness[i].clear();
		depth[i].clear();
		normal[i].clear();
		occlusion[i].clear();
	}
}
/*
void ThrowZlib(int code)
{
	switch(code)
	{
	case Z_OK:
		break;
	case Z_STREAM_END:
		break;
	case Z_NEED_DICT:
		throw std::runtime_error("need dict");
	case Z_ERRNO:
		throw std::system_error(errno, std::generic_category());
	case Z_STREAM_ERROR:
		throw std::runtime_error("zlib: stream error");
	case Z_DATA_ERROR:
		throw std::runtime_error("zlib: data error");
	case Z_MEM_ERROR:
		throw std::runtime_error("zlib: memory error");
	case Z_BUF_ERROR:
		throw std::runtime_error("zlib: buffer error");
	case Z_VERSION_ERROR:
		throw std::runtime_error("zlib: version error");
	default:
		throw std::runtime_error("unknown error");
	}

}*/

#include <sstream>
#include <iostream>
#include <iomanip>

std::string ToHex(const uint8_t * s, int length, bool upper_case  = true)
{
    std::ostringstream ret;

    for (int i = 0; i < length; ++i)
        ret << std::hex << std::setfill('0') << std::setw(2) << (upper_case ? std::uppercase : std::nouppercase) << (int)s[i];

    return ret.str();
}

#define UNIT_TEST   0

void BackgroundFile::Compress()
{
#if VERSION < 3
	return;
#endif

	enum
	{
		BUFFER_SIZE = 350000,
		DEPTH_BUFFER_SIZE = 2 * 256 * 256 * sizeof(uint16_t),
		TOTAL_SIZE = BUFFER_SIZE + DEPTH_BUFFER_SIZE,
		P_LENGTH    = 20
	};

	auto output_size = LZ4_compressBound(TOTAL_SIZE);
	std::vector<uint8_t> output_block(output_size);

	int total_steps = MAX_MIP * length(), step = 0, progress = 0;
	char progress_bar[P_LENGTH];
	memset(progress_bar, ' ', P_LENGTH);

	printf("compressing image...\n");
	printf("[%.*s]\r", P_LENGTH, progress_bar);

	for(auto i = 0; i < MAX_MIP; ++i)
	{
		if(encoded[i] == nullptr)
			continue;

		for(int j = 0; j < length(); ++j, ++step)
		{
			int percentage = (step + P_LENGTH/2) * P_LENGTH / total_steps;

			if (percentage > progress)
			{
				progress_bar[(progress = percentage)-1] = '=';
				printf("[%.*s]\r", P_LENGTH, progress_bar);
			}

			if(encoded[i][j].empty()) continue;

			output_block.resize(LZ4_compressBound(TOTAL_SIZE));
			int total_out = LZ4_compress_HC(
				(const char*)&encoded[i][j][0],
				(char*)output_block.data(),
				encoded[i][j].size(),
				output_block.size(),
				LZ4HC_CLEVEL_MAX);

			output_block.resize(total_out);

			encoded[i][j].swap(output_block);
		}
	}

	printf("\n");

/*
#define UNIT_TEST   0

	z_stream zlib;
	memset(&zlib, 0, sizeof(zlib));
	int code = deflateInit(&zlib, Z_BEST_COMPRESSION);
	ThrowZlib(code);

	zlib.data_type = Z_BINARY;

	std::unique_ptr<uint8_t[]> output_block(new uint8_t[TOTAL_SIZE]);

#if UNIT_TEST
	std::unique_ptr<uint8_t[]> decompress_block(new uint8_t[BUFFER_SIZE]);
#endif

	int total_steps = MAX_MIP * length(), step = 0, progress = 0;
	char progress_bar[P_LENGTH];
	memset(progress_bar, ' ', P_LENGTH);

	printf("compressing image...\n");
	printf("[%.*s]\r", P_LENGTH, progress_bar);

	for(auto i = 0; i < MAX_MIP; ++i)
	{
		if(encoded[i] == nullptr)
			continue;

		for(int j = 0; j < length(); ++j, ++step)
		{
			int percentage = (step + P_LENGTH/2) * P_LENGTH / total_steps;

			if (percentage > progress)
			{
				progress_bar[(progress = percentage)-1] = '=';
				printf("[%.*s]\r", P_LENGTH, progress_bar);
			}

			if(encoded[i][j].empty()) continue;

			zlib.next_in  = &encoded[i][j][0];
			zlib.avail_in = encoded[i][j].size();
			zlib.total_in = 0;

			zlib.next_out  = &output_block[0];
			zlib.avail_out = TOTAL_SIZE;
			zlib.total_out = 0;

//need random reads...
			int code = deflate(&zlib, Z_FULL_FLUSH);

			if(zlib.msg != nullptr)
				throw std::runtime_error(zlib.msg);

			ThrowZlib(code);

#if UNIT_TEST
			z_stream alib;
			memset(&alib, 0, sizeof(alib));
			inflateInit(&alib);

			alib.next_in = &output_block[0];
			alib.avail_in = zlib.total_out;
			alib.total_in = 0;

			alib.next_out = &decompress_block[0];
			alib.avail_out = BUFFER_SIZE;
			alib.total_out = 0;

			int code2 = inflate(&alib, Z_FINISH);

			if(alib.msg != nullptr)
				throw std::runtime_error(alib.msg);

			if(alib.total_out != encoded[i][j].size())
				throw std::runtime_error("decompressed size wrong");

			if(memcmp(&encoded[i][j][0], &decompress_block[0], alib.total_out) != 0)
				throw std::runtime_error("decompression failed?");

			inflateEnd(&alib);
#endif
			encoded[i][j].resize(zlib.total_out);
			memcpy(&encoded[i][j][0], &output_block[0], zlib.total_out);

			deflateReset(&zlib);
		}
	}

	printf("\n");

	deflateEnd(&zlib);*/
}

void BackgroundFile::CreateTileDimensions(AlphaFile & alpha_file)
{
	SetTileCount(base_color);
	SetTileCount(depth);
	SetTileCount(normal);
	SetTileCount(roughness);

	tile_info = std::unique_ptr<TileInfo[]>(new TileInfo[length()]);

	if(alpha_file.empty())
	{
		for(int i = 0; i < length(); ++i)
			tile_info[i].flags = (uint16_t)0xFFFFF;
	}
	else
	{
		for(int i = 0; i < length(); ++i)
			tile_info[i] = GetDimensions(alpha_file, i);

		tile_info[length()-1] = GetDimensions(alpha_file, length()-1);
	}

}

BackgroundFile::TileInfo BackgroundFile::GetDimensions(AlphaFile & alpha_file, int i)
{
	BackgroundFile::TileInfo  info;

	for(int y = 0; y < 4; ++y)
	{
		for(int x = 0; x < 4; ++x)
		{
			info.setFlag(x, y, IsSubtileOpaque(alpha_file, x, y, i));
		}
	}

	return info;
}

bool BackgroundFile::IsSubtileOpaque(AlphaFile & alpha_file, int sub_x, int sub_y, int i)
{
	const int start_x = (i % tiles_x) * 256 + sub_x * 64;
	const int start_y = (i / tiles_x) * 256 + sub_y * 64;
	const int end_x = start_x + 64;
	const int end_y = start_y + 64;

	for(int y = start_y; y < end_y; y += 4)
	{
		for(int x = start_x; x < end_x; x += 4)
		{
			if(alpha_file.GetMask(0, x, y))
				return true;
		}
	}

	return false;
}

#if 0
template<typename T>
std::unique_ptr<T[]> ApplyTileDimensions(BackgroundFile::TileInfo & dimensions, std::unique_ptr<T[]> & blocks, int mip)
{
	size_t offset = 4 - mip;

	if(typeid(T) != typeid(uint16_t))
		offset -= 2;

	const uint16_t x0 = dimensions.x0 << offset;
	const uint16_t x1 = dimensions.x1 << offset;
	const uint16_t y0 = dimensions.y0 << offset;
	const uint16_t y1 = dimensions.y1 << offset;

	const int16_t w  = x1 - x0;
	const int16_t h  = y1 - y0;

	std::unique_ptr<T[]> r(new T[w*h]);

	for(int16_t y = 0; y < h; ++y)
	{
		memcpy(&r[y*w], &blocks[(y0 + y)*(64 >> mip) + x0] , w*sizeof(T));
	}

	return r;
}

template<typename T>
std::vector<T> ApplyTileDimensions(BackgroundFile::TileInfo & dimensions, std::vector<T> & blocks, int mip)
{
	size_t offset = 4 - mip;

	if(typeid(T) != typeid(uint16_t))
		offset -= 2;

	const uint16_t x0 = dimensions.x0 << offset;
	const uint16_t x1 = dimensions.x1 << offset;
	const uint16_t y0 = dimensions.y0 << offset;
	const uint16_t y1 = dimensions.y1 << offset;

	const int16_t w  = x1 - x0;
	const int16_t h  = y1 - y0;

	std::vector<T> r(w*h);

	for(int16_t y = 0; y < h; ++y)
	{
		memcpy(&r[y*w], &blocks[(y0 + y)*(64 >> mip) + x0] , w*sizeof(T));
	}

	return r;
}

template<typename T>
void ApplyTileDimensionsTemplate(BackgroundFile * _this, std::unique_ptr<std::unique_ptr<T[]>[]> & it, int mip, int j)
{
	if(it == nullptr)
		return;

	if(_this->tile_info[j].widthBlocks() == 0)
		it[j].reset();
	else
		it[j] = ApplyTileDimensions(_this->tile_info[j], it[j], mip);
}

template<typename T>
void ApplyTileDimensionsTemplate(BackgroundFile * _this, std::vector<std::vector<T>> & it, int mip, int j)
{
	if(it.empty())
		return;

	if(_this->tile_info[j].widthBlocks() == 0)
		it[j].clear();
	else
		it[j] = ApplyTileDimensions(_this->tile_info[j], it[j], mip);
}

void BackgroundFile::ApplyTileDimensions(AlphaFile & alpha_file)
{
	CreateTileDimensions(alpha_file);
/*
	if(max == 0)
		return;

	for(int j = 0; j < length(); ++j)
	{
		if(tile_info[j].widthBlocks() == 16 && tile_info[j].heightBlocks() == 16)
			continue;

		for(int i = 0; i < 3; ++i)
		{
			ApplyTileDimensionsTemplate(this, depth[i], i, j);
			ApplyTileDimensionsTemplate(this, normal[i], i, j);
			ApplyTileDimensionsTemplate(this, base_color[i], i, j);
			ApplyTileDimensionsTemplate(this, roughness[i], i, j);
			ApplyTileDimensionsTemplate(this, occlusion[i], i, j);
		}
	}*/
}
#endif

bool isTransparent(BC5_Block & m)
{
	return isTransparent(m.R) && isTransparent(m.G);
}


bool isTransparent(BC4_Block & m)
{
	static const uint8_t zero_strings[3][6]
	{
		{0, 0, 0, 0, 0, 0},
		{0x24, 0x92, 0x49, 0x24, 0x92, 0x49},
		{0xDB, 0x6D, 0xB6, 0xDB, 0x6D, 0xB6},
	};

	return (!m.c[0] && !m.c[1])
	    || (m.c[0] < m.c[1] && !memcmp(zero_strings[2], m.i, 6))
	    || (!m.c[0] && !memcmp(zero_strings[0], m.i, 6))
	    || (!m.c[1] && !memcmp(zero_strings[1], m.i, 6));
}

bool isTransparent(DXT1_Block & m)
{
	return m.c[0] < m.c[1]
		&& m.i[0] == 0xFF
		&& m.i[1] == 0xFF
		&& m.i[2] == 0xFF
		&& m.i[3] == 0xFF;
}

bool isTransparent(DXT5_Block & m)
{
	return isTransparent(m.alpha);
}
