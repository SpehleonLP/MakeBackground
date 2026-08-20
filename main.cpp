#include "src/backgroundexception.h"
#include "src/backgroundfile.h"
#include "src/png_file.h"
#include "src/depthfile.h"
#include "src/ddsfile.h"
#include "src/alphafile.h"
#include "src/generatenormals.h"
#include "src/generateocclusion.h"
#include "src/gltffile.h"
#include <glm/vec2.hpp>
#include <optional>
#include <sstream>
#include <nfd.h>
#include <boxer/boxer.h>
#include <iostream>
#include <cassert>
#include <thread>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

enum BlockType
{
	ShadowVolume = 0x57444853,
};


bool SetPath(int argc, char ** argv);
std::string GetSavePath(int argc, char *argv[]);
DDSFile GetDDSFile(bg_Type type, DepthFile &, AlphaFile & alpha);
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
#ifdef NDEBUG
	try
	{
#endif
		if(!SetPath(argc, argv))
			return 0;

		DepthFile depthFile;
		AlphaFile alphaMask;

		try
		{
			depthFile = GetDepth();
			alphaMask.Load(depthFile);
		}
		catch(std::exception & e)
		{
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "%s, Create unlit background?", e.what());
			auto selection = boxer::show(buffer, "MakeBackground", boxer::Style::Question, boxer::Buttons::YesNo);

			if(selection == boxer::Selection::No)
				return 0;

			DDSFile BaseColor = GetDDSFile(bg_Type::Diffuse, depthFile, alphaMask);

			std::string save_path = GetSavePath(argc, argv);

			if (save_path.empty())
				return 0;

			BackgroundFile bg(std::move(save_path));

			bg.base_color.SetFile(BaseColor);

			bg.CreateTileDimensions(alphaMask);

			bg.Deinterleave();
			bg.Compress();

			bg.WriteOut();

			return 0;
		}

		std::array<std::optional<DDSFile>, 5> _files;

#if 0
		for(int i = 0; i < 5; ++i)
		{
			_files[i] = GetDDSFile((bg_Type)i, depthFile, alphaMask);
		}
#else
		{
			std::array<std::unique_ptr<std::thread>, 5> _getters;

			for(int i = 0; i < 5; ++i)
			{
				_getters[i] = std::make_unique<std::thread>([&_files, &depthFile, &alphaMask, i] { _files[i] = GetDDSFile((bg_Type)i, depthFile, alphaMask); });
			}

			bool need_stop = false;
			for(auto i = 0u; i < _getters.size(); ++i)
			{
				if(_getters[i])
				{
					_getters[i]->join();

					if(!_files[i])
						need_stop = true;
				}
			}

			if(need_stop)
				return -1;
		}
#endif


		DDSFile & BaseColor = *_files[int(bg_Type::Diffuse)];
		DDSFile & Depth     = *_files[int(bg_Type::Depth)];
		DDSFile & Normals   = *_files[int(bg_Type::Normals)];
		DDSFile & Occlusion = *_files[int(bg_Type::Occlusion)];
		DDSFile & Roughness = *_files[int(bg_Type::Roughness)];
		auto gltfShadow = gltfFile("ShadowVolume.glb");

		std::string save_path = GetSavePath(argc, argv);

		if (save_path.empty())
			return 0;

		BackgroundFile bg(std::move(save_path));
/*
		if(bg.moreRecent(BaseColor)
		&& bg.moreRecent(Depth)
		&& bg.moreRecent(Normals)
		&& bg.moreRecent(Roughness)
		&& bg.moreRecent(Occlusion)
		&& bg.moreRecent(gltfShadow))
			return 0;*/

		gltfShadow.LoadItUp();
		gltfShadow.Compress();

		bg.base_color.SetFile(BaseColor);
		bg.depth.SetFile(Depth);
		bg.normal.SetFile(Normals);
		bg.occlusion.SetFile(Occlusion);
		bg.roughness.SetFile(Roughness);

		bg.CreateTileDimensions(alphaMask);

		bg.Deinterleave();
		bg.Compress();

		if(gltfShadow.doesExist())
			bg.Append(ShadowVolume, std::move(gltfShadow.compressed));

		bg.WriteOut();

#ifdef NDEBUG
	}
	catch(std::exception & e)
	{
		boxer::show(e.what(), "MakeBackground", boxer::Style::Error, boxer::Buttons::Quit);
	}
#endif

	return 0;
}

DepthFile GetDepth()
{
	DepthFile depth_map;

	PngFile depth("Depth.png", bg_Type::Depth, 0);

	if (depth.doesExist() == false)
	{
		throw std::runtime_error("Unable to locate Depth.png, stopping.");
	}

	depth_map.ReadHeader();
	depth_map.Load();

//create platform maps
	GetPngFile("Platform", bg_Type::Platform, 3, depth_map);

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

DDSFile GetDDSFile(bg_Type type, DepthFile & depth, AlphaFile & alpha)
{
	auto name = bg_FileName(type);
	char file_name[64];
	snprintf(file_name, 64, "%s.dds", name);

	PngFile mip0 = GetPngFile(name, type, 0, depth);
	PngFile mip1 = GetPngFile(name, type, 1, depth);
	PngFile mip2 = GetPngFile(name, type, 2, depth);
	PngFile mip3 = GetPngFile(name, type, 3, depth);

	PngFile * mip[4];

	mip[0] = &mip0;
	mip[1] = &mip1;
	mip[2] = &mip2;
	mip[3] = &mip3;

	DDSFile file(file_name);

	if(mip0.doesExist() == false)
		return file;

	if(file.moreRecent(mip0)
	&& file.moreRecent(mip1)
	&& file.moreRecent(mip2)
	&& file.moreRecent(mip3))
		return file;

	std::cout << "Generating " << bg_TypeName(type) << ".DDS..." << std::endl;

	file.create(mip, type, 4, alpha);
	file.Write();

	return file;
}

PngFile LocatePngFile(const char * name, bg_Type type, int mip)
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

	return file;
}

void CreateMipMap(PngFile & file, const char * name, bg_Type type, int mip, DepthFile & depth)
{
	if(mip == 0 && type == bg_Type::Platform)
	{
		if(!depth.doesExist())
			throw std::logic_error("Cannot create platform map without depth map");

		if(!file.moreRecent(depth.modified))
		{
			depth.CopyPlatform(file, mip);
			file.Write();
		}
	}
	else if(mip != 0)
	{
		PngFile base = GetPngFile(name, type, mip-1, depth);

		if(base.moreRecent(file))
		{
			std::cout << "Generating " << bg_TypeName(type) << "-mip" << mip << ".gen.png" << std::endl;

			if(type == bg_Type::Platform)
			{
				if(!depth.doesExist())
					throw std::logic_error("Cannot create platform map without depth map");

				depth.CopyPlatform(file, mip);
			}
			else
				file.Scale(depth, base);

			file.Write();
		}
	}
}

void ValidatePngFileSize(PngFile & file, bg_Type type, int mip, DepthFile & depth)
{
	char buffer[128];

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

			if(!depth.doesExist())
				return;

			depth.ReadHeader();

			if(file.width() << mip != depth.size.x
			|| file.height() << mip != depth.size.y)
			{
				snprintf(buffer, sizeof(buffer), "Mip layer %i of %s has incorrect dimensions (does not match depth data).", mip, bg_TypeName(type));
				throw BackgroundException(buffer);
			}
		}
	}
}


PngFile GetPngFile(const char * name, bg_Type type, int mip, DepthFile & depth)
{
	auto file = LocatePngFile(name, type, mip);
	CreateMipMap(file, name, type, mip, depth);
	ValidatePngFileSize(file, type, mip, depth);
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

std::string ConfirmExtension(std::string && str)
{
	if(str.empty())
		return "";

	auto pos = str.find_last_of('.');

	if(str.size() - pos != 7)
		return str + ".lf_bck";

	char buffer[6];
	for(uint32_t i = pos+1; i < str.size(); ++i)
		buffer[i-(pos+1)] = tolower(str[i]);

	if(memcmp(buffer, "lf_bck", 6) == 0)
		return std::move(str);

	return str + ".lf_bck";
}

std::string GetSavePath(int argc, char *argv[])
{
	if(argc > 2)
		return ConfirmExtension(argv[2]);

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
	return ConfirmExtension(std::move(path));
}

