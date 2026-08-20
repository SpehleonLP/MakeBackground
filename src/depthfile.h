#ifndef DEPTHFILE_H
#define DEPTHFILE_H
#include <glm/gtc/type_precision.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include <string>
#include <vector>

class PngFile;

class DepthFile
{
public:
	enum
	{
		MAX_DEPTH = 64
	};

	DepthFile();

	void Load();
	void clear();

	std::string path;
	time_t      modified{};

// red = same platform as us
// green
	std::vector<float>			   m_height;
	glm::ivec2                     size{0, 0};

	bool doesExist() const;
	size_t GetOffset(int mip) const;

	float const* GetHeight(int mip); // { return height.get(); }
 //     uint32_t * GetPlatform(int mip);
	const uint32_t * GetPlatformMask(int mip);

	void DownscalePlatform(int mip);

	//either in the red channel or greyscale image
	void ReadHeader();
	void CopyPlatform(PngFile & file, int mip);

	//either in green + blue channels or greyscale image
	void AddDepth(const char * name, float multiple, bool needFile);
	void AddDepth(PngFile const& file, float multiple);
	//void WriteDepth(PngFile & file, float limit);

private:
	void CheckDepth(const char * name);
	PngFile LocateDepth(const std::string & name, bool needFile);

	std::unique_ptr<uint32_t[]>    platform_mask;
	uint32_t platformMaskMask{};
	mutable int8_t   m_exists{-1};
};

#endif // DEPTHFILE_H
