#include "png_file.h"

/*
 * Copyright 2002-2010 Guillaume Cottenceau.
 *
 * This software may be freely redistributed under the terms
 * of the X11 license.
 *
 */

#include <system_error>
#include "backgroundexception.h"
#include "linearimage.h"
#include "depthfile.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <sys/types.h>
#include <cmath>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#else
#define M_PI 3.14159265358
#endif

#define PNG_DEBUG 3
#include <png.h>

thread_local png_const_charp g_error_msg = "";

void user_error_fn(png_structp png_ptr, png_const_charp error_msg)
{
	g_error_msg = error_msg;
	longjmp(png_jmpbuf(png_ptr), 1);
}

void user_warning_fn(png_structp , png_const_charp warning_msg)
{
	fprintf(stderr, "%s\n", warning_msg);
}

FILE * open_file(const char* path, const char * mode)
{
	FILE* r{};

#ifdef _WIN32
	errno_t e = fopen_s(&r, path, mode);

	if (e)
		throw std::system_error(e, std::generic_category(), path);
#else
	r = fopen(path, mode);

	if (r == nullptr)
		throw std::system_error(errno, std::generic_category(), path);
#endif

	return r;
}

PngFile::PngFile(PngFile && it)  :
	FileBase(std::move(it)),
	type(it.type),
	mip(it.mip),
	size(it.size),
	color_type(it.color_type),
	bit_depth(it.bit_depth),
	channels(it.channels),
	row_pointers(it.row_pointers)
{
	it.row_pointers = nullptr;
}

PngFile::~PngFile()
{
	clear();
}

void PngFile::Alloc()
{
	if(image == nullptr)
	{
		image = (png_bytep) malloc((sizeof(void*) + bytesPerRow) * height());
		row_pointers = (png_bytep*) (image + (height() * bytesPerRow));

		for (int y=0; y<height(); ++y)
			row_pointers[y] = image + bytesPerRow*y;
	}
}

std::unique_ptr<png_bytep[]> PngFile::GetInvRows()
{
	if(image == nullptr)
		return nullptr;

	std::unique_ptr<png_bytep[]> r(new png_bytep[height()]);

	for(int y = 0; y < height(); ++y)
		r[(height()-1) - y] = row_pointers[y];

	return r;
}

void PngFile::clear()
{
	if(row_pointers != nullptr)
	{
		free(image);
		image        = nullptr;
		row_pointers = nullptr;
	}
}

void PngFile::ReadHeader(FILE * fp, png_structp& png_ptr, png_infop& info_ptr)
{
	char header[8];

/* open file and test for it being a png */

	if (!fp)
		throw std::system_error(errno, std::generic_category());

	try
	{
		fread(header, 1, 8, fp);
		if (png_sig_cmp((png_bytep)header, 0, 8))
				throw LibPngException("[read_png_file] File is not a PNG file");

		/* initialize stuff */
		png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

		if (!png_ptr)
		throw LibPngException("[read_png_file] png_create_read_struct failed");

		info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr)
		throw LibPngException("[read_png_file] png_create_info_struct failed");

		if (setjmp(png_jmpbuf(png_ptr)))
		throw LibPngException("[read_png_file] Error during init_io");

		png_set_error_fn(png_ptr, nullptr, user_error_fn, user_warning_fn);

		png_init_io(png_ptr, fp);
		png_set_sig_bytes(png_ptr, 8);

		png_read_info(png_ptr, info_ptr);
		png_set_swap(png_ptr);

		size.x		= png_get_image_width(png_ptr, info_ptr);
		size.y		= png_get_image_height(png_ptr, info_ptr);
		color_type  = (ColorType) png_get_color_type(png_ptr, info_ptr);
		bit_depth	= png_get_bit_depth(png_ptr, info_ptr);
		channels	= png_get_channels(png_ptr, info_ptr);
		bytesPerRow = png_get_rowbytes(png_ptr,info_ptr);

		if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE)
			png_set_palette_to_rgb(png_ptr);
	/*
		if(isGrayScale())
		{
			if (setjmp(png_jmpbuf(png_ptr)))
				throw std::runtime_error("[read_png_file] expected grayscale image");

			if(png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB
			||png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB_ALPHA)
				png_set_rgb_to_gray_fixed(png_ptr, 2, -1, -1);
		}*/

		if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY &&  bit_depth < 8)
		{
			png_set_expand_gray_1_2_4_to_8(png_ptr);
			bit_depth = 8;
		}

		if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
			png_set_tRNS_to_alpha(png_ptr);

		if(type == bg_Type::Occlusion)
		{
			int break_point = 0;
			++break_point;

		}

		if(type == bg_Type::Depth || type == bg_Type::Occlusion)
		{
			if (color_type == ColorType::RGB ||
				color_type == ColorType::RGBA)
			{
		//		png_set_rgb_to_gray_fixed(png_ptr, 1, -1,- 1);
		//		color_type = ColorType::GRAY;
		//		channels   = 1;
			}
		}

		if(type == bg_Type::Depth && png_get_bit_depth(png_ptr, info_ptr) < 16)
		{
			png_set_expand_16(png_ptr);
			bit_depth = 16;
		}

	//	if (png_get_bit_depth(png_ptr, info_ptr) == 16)	png_set_swap(png_ptr);
	}
	catch(...)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
		throw;
	}
}

void PngFile::ReadHeader()
{
	png_structp png_ptr{};
	png_infop info_ptr{};

	/* open file and test for it being a png */
	try
	{
		if(row_pointers != nullptr || doesExist() == false)
			return;

		FileHandle handle(open_file(path.c_str(), "rb"));
		ReadHeader(handle.get(), png_ptr, info_ptr);
	}
	catch(std::exception const& e)
	{
		throw FileException("Couldn't open: " + path + "\n  " + e.what());
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
}

void PngFile::Read()
{
	if(row_pointers != nullptr || doesExist() == false)
		return;

	int block_size = 16 >> mip;

	png_structp png_ptr{};
	png_infop info_ptr{};

	/* open file and test for it being a png */
	FileHandle handle(open_file(path.c_str(), "rb"));

	try
	{
		ReadHeader(handle.get(), png_ptr, info_ptr);

		if(width() % block_size != 0
		|| height() % block_size != 0)
			throw BackgroundException("dimensions must be easily divisible by " + std::to_string(block_size));

        png_set_interlace_handling(png_ptr);
        png_read_update_info(png_ptr, info_ptr);

        /* read file */
        if (setjmp(png_jmpbuf(png_ptr)))
		{
            throw LibPngException("[read_png_file] Error during read_image: ", g_error_msg);
		}

		Alloc();

		auto invRows = GetInvRows();

        png_read_image(png_ptr, invRows.get());
	}
	catch(std::exception const& e)
	{
		throw FileException("Couldn't open: " + path + "\n  " + e.what());
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
}

constexpr float lancoz(float x, float a)
{
	return a * std::sin(M_PI * x) * std::sin(M_PI * x / a) / (M_PI * M_PI * x * x);
}

void PngFile::Scale(DepthFile & depth, PngFile & source)
{
	static std::array<float, 4> kernel{
		lancoz(-1.5, 2),
		lancoz(- .5, 2),
		lancoz(- .5, 2),
		lancoz( 1.5, 2) };

	LinearImage img;

	img.fromPng(source);
	img.toLinear();
	img.LinearDownscale(depth.GetPlatformMask(source.mip), kernel);
	img.fromLinear();
	img.toPng(*this);


#if 0
	it.Read();
	clear();

	width  = it.width >> 1;
	height = it.height >> 1;

	color_type = it.color_type;
	bit_depth  = it.bit_depth;
	channels   = it.channels;

	row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
	for (int y=0; y<height; y++)
		row_pointers[y] = (png_byte*) malloc(width * channels * (bit_depth / 8));

	DownScalePNG(row_pointers, it.row_pointers, it.width, it.height, it.channels, it.bit_depth);
#endif
}

void PngFile::Write()
{
	png_structp png_ptr{};
	png_infop info_ptr{};

	try
	{
        /* create file */
		FileHandle handle(open_file(path.c_str(), "wb"));
		auto fp = handle.get();

		if (!fp)
			throw std::system_error(errno, std::generic_category());

        /* initialize stuff */
        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!png_ptr)
                throw LibPngException("[write_png_file] png_create_write_struct failed");

        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr)
                throw LibPngException("[write_png_file] png_create_info_struct failed");

        if (setjmp(png_jmpbuf(png_ptr)))
                throw LibPngException("[write_png_file] Error during init_io");

        png_init_io(png_ptr, fp);


        /* write header */
        if (setjmp(png_jmpbuf(png_ptr)))
			throw LibPngException("[write_png_file] Error during writing header");

        png_set_IHDR(png_ptr, info_ptr, width(), height(),
                     bit_depth, (png_byte) color_type, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

        png_write_info(png_ptr, info_ptr);
		png_set_swap(png_ptr);

        /* write bytes */
        if (setjmp(png_jmpbuf(png_ptr)))
			throw LibPngException("[write_png_file] Error during writing bytes");

		auto invRows = GetInvRows();
		png_write_image(png_ptr, invRows.get());

        /* end write */
        if (setjmp(png_jmpbuf(png_ptr)))
                throw LibPngException("[write_png_file] Error during end of write");

        png_write_end(png_ptr, NULL);
	}
	catch(std::exception const& e)
	{
		png_destroy_write_struct(&png_ptr, &info_ptr);
		throw FileException("Couldn't save: " + path + "\n  " + e.what());
	}

	png_destroy_write_struct(&png_ptr, &info_ptr);
}
