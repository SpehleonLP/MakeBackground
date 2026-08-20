#ifndef BG_TYPE_H
#define BG_TYPE_H

enum class bg_Type : int
{
	Diffuse,
	Depth,
	Normals,
	Roughness,
	Occlusion,
	Platform
};

const char * bg_TypeName(bg_Type);
const char * bg_FileName(bg_Type);


#endif // BG_TYPE_H
