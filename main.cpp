#include "src/backgroundexception.h"
#include "src/backgroundfile.h"
#include "src/png_file.h"
#include "src/depthfile.h"
#include "src/ddsfile.h"
#include "src/generatenormals.h"
#include "src/generateocclusion.h"
#include <glm/vec2.hpp>
#include <sstream>
#include <nfd.h>
#include <boxer/boxer.h>
#include <iostream>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif


bool SetPath(int argc, char ** argv);
std::string GetSavePath();
DDSFile GetDDSFile(const char * name,  bg_Type type, DepthFile &);
PngFile GetPngFile(const char * name, bg_Type type, int mip, DepthFile &);


DepthFile GetDepth();

void MakeSphere(const int width, const int height)
{
	PngFile png("SphereHeightMap.png", bg_Type::Depth, 0);

	png.size        = glm::ivec2(width, height);
	png.color_type  = PngFile::ColorType::GRAY;
	png.bit_depth   = 16;
	png.channels    = 1;
	png.bytesPerRow = 2 * width;

	png.Alloc();

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
	}

	png.Write();
}

int main(int argc, char *argv[])
{
//	MakeSphere(2048, 2048);

	try
	{
		if(!SetPath(argc, argv))
			return 0;

		auto depth_file  = GetDepth();

		DDSFile Depth     = GetDDSFile("Depth",            bg_Type::Depth, depth_file);
		DDSFile Normals   = GetDDSFile("Normals",          bg_Type::Normals, depth_file);
		DDSFile Occlusion = GetDDSFile("AmbientOcclusion", bg_Type::Occlusion, depth_file);

		DDSFile BaseColor = GetDDSFile("BaseColor",        bg_Type::Diffuse, depth_file);
		DDSFile Roughness = GetDDSFile("Roughness",        bg_Type::Roughness, depth_file);

		std::string save_path = GetSavePath();

		if (save_path.empty())
			return 0;

		BackgroundFile bg(std::move(save_path));

		if(bg.moreRecent(BaseColor)
		&& bg.moreRecent(Depth)
		&& bg.moreRecent(Normals)
		&& bg.moreRecent(Roughness)
		&& bg.moreRecent(Occlusion))
			return 0;

		bg.base_color.SetFile(BaseColor);
		bg.depth.SetFile(Depth);
		bg.normal.SetFile(Normals);
		bg.roughness.SetFile(Occlusion);
		bg.roughness.SetFile(Roughness);

		bg.CreateTileDimensions();
		bg.ApplyTileDimensions();

		bg.Deinterleave();
		bg.Compress();

		bg.WriteOut();
	}
	catch(std::exception & e)
	{
		boxer::show(e.what(), "MakeBackground", boxer::Style::Error, boxer::Buttons::Quit);
	}

	return 0;
}

DepthFile GetDepth()
{
	glm::ivec2 size;

	DepthFile depth_map;

	PngFile depth("Depth.png", bg_Type::Depth, 0);

	if (depth.doesExist() == false)
	{
		if (!depth.ChangePath("Depth.gen.png")
			|| depth.modified < depth_map.modified)
		{
			std::cout << "Generating Depth.gen.png" << std::endl;
			depth_map.WriteDepth(depth, 5);
		}
	}

//------------------------
// Generate Normals
//------------------------
	PngFile normals("Normals.png", bg_Type::Normals, 0);

	if (normals.doesExist() == false)
	{
		if (!normals.ChangePath("Normals.gen.png")
			|| normals.modified < depth_map.modified)
		{
			std::cout << "Generating Normals.gen.png" << std::endl;
			GenerateNormals(normals, depth_map);
		}
	}

//------------------------
// Generate AO
//------------------------
	PngFile occlusion = PngFile("AmbientOcclusion.png", bg_Type::Occlusion, 0);

	if(occlusion.doesExist() == false
	&& occlusion.ChangePath("AmbientOcclusion.gen.png"))
		GenerateOcclusion(occlusion, depth_map, normals, 32);

	depth_map.clear();

	return depth_map;
}

DDSFile GetDDSFile(const char * name, bg_Type type, DepthFile & depth)
{
	char file_name[64];
	snprintf(file_name, 64, "%s.dds", name);

	PngFile mip0 = GetPngFile(name, type, 0, depth);
	PngFile mip1 = GetPngFile(name, type, 1, depth);
	PngFile mip2 = GetPngFile(name, type, 2, depth);
	PngFile mip3 = GetPngFile(name, type, 3, depth);

	DDSFile file(file_name);

	if(mip0.doesExist() == false)
		return file;

	if(file.moreRecent(mip0)
	&& file.moreRecent(mip1)
	&& file.moreRecent(mip2)
	&& file.moreRecent(mip3))
		return file;

	PngFile * mip[4];

	mip[0] = &mip0;
	mip[1] = &mip1;
	mip[2] = &mip2;
	mip[3] = &mip3;

	std::cout << "Generating " << bg_TypeName(type) << ".DDS..." << std::endl;

	file.create(mip, type, 4);
	file.Write();

	return file;
}


PngFile GetPngFile(const char * name, bg_Type type, int mip, DepthFile & depth)
{
	char buffer[128];

	if(mip == 0)
		snprintf(buffer, sizeof(buffer), "%s.png", name);
	else
		snprintf(buffer, sizeof(buffer), "%s-mip%d.png", name, mip);

	PngFile file(buffer, type, mip);

	if(file.doesExist() == false)
	{
		if(mip == 0)
			snprintf(buffer, sizeof(buffer), "%s.gen.png", name);
		else
			snprintf(buffer, sizeof(buffer), "%s-mip%d.gen.png", name, mip);

		file.ChangePath(buffer);
	}

	if(mip != 0)
	{
		PngFile base = GetPngFile(name, type, mip-1, depth);

		if(base.moreRecent(file))
		{
			std::cout << "Generating " << bg_TypeName(type) << "-mip" << mip << ".gen.png" << std::endl;

			file.Scale(depth, base);
			file.Write();
		}
	}

	if(file.doesExist())
	{
		file.ReadHeader();

		if(file.width() != 0 && file.height() != 0)
		{
			if((file.width() % (256 >> mip)) != 0
			&& (file.height() % (256 >> mip)) != 0)
			{
				snprintf(buffer, sizeof(buffer), "Mip layer %i of %s map must have both a width and height which are multiples of %i", mip, bg_TypeName(type), 256 >> mip);
				throw BackgroundException(buffer);
			}

			depth.ReadPlatformHeader();

			if(file.width() << mip != depth.size.x
			|| file.height() << mip != depth.size.y)
			{
				snprintf(buffer, sizeof(buffer), "Mip layer %i of %s has incorrect dimensions (does not match depth data).", mip, bg_TypeName(type));
				throw BackgroundException(buffer);
			}
		}
	}

	return file;
}

#ifndef _WIN32
#define _chdir chdir
#endif


bool SetPath(int argc, char ** argv)
{
	std::string path;

	if(argc > 1)
		path = argv[1];
	else
	{
		char * buffer = nullptr;

		switch(NFD_PickFolder(nullptr, &buffer))
		{
		case NFD_ERROR:
			throw BackgroundException(NFD_GetError());
		case NFD_OKAY:
			break;
		case NFD_CANCEL:
			return false;
		default:
			break;
		}

		path = buffer;
		free(buffer);
	}

	if (-1 == _chdir(path.c_str()))
		throw BackgroundException("Unable to set working directory to: '" + path + '\'');

	return true;
}

std::string GetSavePath()
{
	std::string path;

	char* buffer = nullptr;

	switch (NFD_SaveDialog("lf_bck", nullptr, &buffer))
	{
	case NFD_ERROR:
		throw BackgroundException(NFD_GetError());
	case NFD_OKAY:
		break;
	case NFD_CANCEL:
		return {};
	default:
		break;
	}

	path = buffer;
	free(buffer);
	return path;
}

