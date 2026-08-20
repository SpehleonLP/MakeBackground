#ifndef GLTFFILE_H
#define GLTFFILE_H
#include <glm/gtc/type_precision.hpp>
#include "filebase.h"
#include <vector>

class gltfFile : public FileBase
{
public:
	gltfFile(const char * path) : FileBase(path) {}
	~gltfFile() = default;

	void clear() { vertices.clear(); indices.clear(); }
	bool isBinaryFile() const;

	void LoadItUp();
	void Compress();

	std::vector<glm::vec3>  vertices;
	std::vector<int>		   indices;

	std::vector<uint8_t>	   compressed;
};

#endif // GLTFFILE_H
