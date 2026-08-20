#ifndef BLURHEIGHTMAP_H
#define BLURHEIGHTMAP_H
#include <vector>

class PngFile;
class DepthFile;
struct BlurStage;

//void BlurHeightMap(PngFile & dst, PngFile & src, DepthFile & depth, std::vector<BlurStage> const& stages);

enum
{
	kCornerMask = 0x55,
	kSideMask   = 0xAA,
};

#endif // BLURHEIGHTMAP_H
