#include "gltffile.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include "fx/gltf.h"
#include "src/glm_iostream.hpp"
#include <iostream>
#include <climits>

#include "lz4/lib/lz4.h"
#include "lz4/lib/lz4hc.h"

typedef fx::gltf::Accessor::Type			Type;
typedef fx::gltf::Accessor::ComponentType	ComponentType;
typedef fx::gltf::BufferView::TargetType	TargetType;
typedef fx::gltf::Primitive::Mode Mode;

static void EvaluateScene(gltfFile * This,  fx::gltf::Document & doc, fx::gltf::Scene & scene);
static void EvaluateNode(gltfFile * This,  fx::gltf::Document & doc, uint32_t i, const glm::mat4 & mat = glm::mat4(1));
static void EvaluateMesh(gltfFile * This,  fx::gltf::Document & doc, fx::gltf::Mesh mesh, const glm::mat4 & mat = glm::mat4(1));
static void EvaluateIndices(gltfFile * This, fx::gltf::Document & doc, int indices, int vertexOffset, size_t noVertices, Mode mode);
static void EvaluateVertices(gltfFile * This, fx::gltf::Document & doc, int accessorId, glm::mat4 const& mat);

static glm::mat4 GetMatrix(std::array<float, 3> const& translation, std::array<float, 4> const& rotation, std::array<float, 3> scaling);


void gltfFile::LoadItUp()
{
	if(!doesExist()) return;

	fx::gltf::Document doc;

	if(isBinaryFile())	doc = fx::gltf::LoadFromBinary(path);
	else				doc = fx::gltf::LoadFromText(path);

	if((uint32_t)doc.scene < doc.scenes.size())
	{
		EvaluateScene(this, doc, doc.scenes[doc.scene]);
	}
	else if(doc.scenes.size())
	{
		for(auto & scene : doc.scenes)
			EvaluateScene(this, doc, scene);
	}
	else
	{
		for(auto & mesh : doc.meshes)
			EvaluateMesh(this, doc, mesh);
	}
}

bool gltfFile::isBinaryFile() const
{
	std::string p = path;

	for(auto & c : p)
		c = tolower(c);

	return p.find(".glb") != std::string::npos;
}

static void EvaluateScene(gltfFile * This,  fx::gltf::Document & doc, fx::gltf::Scene & scene)
{
	for(auto node : scene.nodes)
	{
		if(node < doc.nodes.size())
			EvaluateNode(This, doc, node);
	}
}

static void EvaluateNode(gltfFile * This, fx::gltf::Document & doc, uint32_t n, glm::mat4 const& mat)
{
	if(n >= doc.nodes.size()) return;

	glm::mat4 node_matrix;
	memcpy(&node_matrix[0][0], doc.nodes[n].matrix.data(), sizeof(glm::mat4));

	node_matrix *= GetMatrix(
		doc.nodes[n].translation,
		doc.nodes[n].rotation,
		doc.nodes[n].scale);

	node_matrix = mat * node_matrix;

	for(auto c : doc.nodes[n].children)
		EvaluateNode(This, doc, c, node_matrix);

	if((uint32_t)doc.nodes[n].mesh < doc.meshes.size())
		EvaluateMesh(This, doc, doc.meshes[doc.nodes[n].mesh], node_matrix);
}

static glm::mat4 GetMatrix(std::array<float, 3> const& translation, std::array<float, 4> const& rotation, std::array<float, 3> scaling)
{
	const glm::vec3 & t = *(glm::vec3*)&translation[0];
	const glm::quat & r = *(glm::quat*)&rotation[0];
	const glm::vec3 & s = *(glm::vec3*)&scaling[0];

	return glm::mat4(
       (1.0 - (2.0 * r.y * r.y) - (2.0 * r.z * r.z)) * s.x,
       (      (2.0 * r.x * r.y) + (2.0 * r.w * r.z)) * s.y,
       (      (2.0 * r.x * r.z) - (2.0 * r.w * r.y)) * s.z,
        0.0,

       (      (2.0 * r.x * r.y) - (2.0 * r.w * r.z)) * s.x,
       (1.0 - (2.0 * r.x * r.x) - (2.0 * r.z * r.z)) * s.y,
       (      (2.0 * r.y * r.z) + (2.0 * r.w * r.x)) * s.z,
        0.0,

       (      (2.0 * r.x * r.z) + (2.0 * r.w * r.y)) * s.x,
       (      (2.0 * r.y * r.z) - (2.0 * r.w * r.x)) * s.y,
       (1.0 - (2.0 * r.x * r.x) - (2.0 * r.y * r.y)) * s.z,
        0.0,

        t.x,
        t.y,
        t.z,
        1);
}

static void EvaluateMesh(gltfFile * This,  fx::gltf::Document & doc, fx::gltf::Mesh mesh, glm::mat4 const& mat)
{
	for(auto const& primitive : mesh.primitives)
	{
		auto position = primitive.attributes.find("POSITION");

		if(position != primitive.attributes.end())
		{
			auto vertexOffset = This->vertices.size();
			EvaluateVertices(This, doc, position->second, mat);
			EvaluateIndices(This, doc, primitive.indices, vertexOffset, This->vertices.size() - vertexOffset, primitive.mode);
		}
	}
}

static void EvaluateVertices(gltfFile * This, fx::gltf::Document & doc, int accessorId, glm::mat4 const& mat)
{
	auto const& accessor = doc.accessors[accessorId];

	if((uint32_t)accessor.bufferView > doc.bufferViews.size())
		throw std::runtime_error("gltf file has bad bufferView index in accessor");

	if(accessor.type != Type::Vec3 || accessor.componentType != ComponentType::Float)
		throw std::runtime_error("gltf file position array must have type Vec3 (Float)");

	auto const& bufferView = doc.bufferViews[accessor.bufferView];

	if((uint32_t)bufferView.buffer > doc.buffers.size())
		throw std::runtime_error("gltf file has bad buffer index in bufferView");

	if(bufferView.target != TargetType::None && bufferView.target != TargetType::ArrayBuffer)
		throw std::runtime_error("gltf file index buffer bufferView has bad target type");

	uint32_t byteStride = bufferView.byteStride? bufferView.byteStride : sizeof(glm::vec3);

	auto const& buffer = doc.buffers[bufferView.buffer];

	if(buffer.data.size() < bufferView.byteOffset + bufferView.byteLength + accessor.byteOffset)
		throw std::runtime_error("gltf file bufferView has bad offset");

	uint8_t const* ptr = &buffer.data[bufferView.byteOffset + accessor.byteOffset];

	auto start = This->vertices.size();
	This->vertices.resize(This->vertices.size() + accessor.count);
	auto begin = This->vertices.data() + start;

	for(uint32_t i = 0; i < accessor.count; ++i)
	{
		glm::vec3 vertex = mat * glm::vec4(*(glm::vec3*)(ptr + (byteStride*i)), 1);
		begin[i] = vertex * 512.f;
	}
}

template<typename T>
static void EvaluateIndices(gltfFile * This, T const* indices, size_t count, size_t byteLength, int vertexOffset, size_t noVertices, Mode mode)
{
	if(sizeof(T) * count > byteLength)
		throw std::runtime_error("gltf file index buffer has bad byteLength (incompatible with count)");

	auto start = This->indices.size();

	for(uint32_t i = 0; i < count; ++i)
	{
		if((size_t)indices[i] >= noVertices)
			throw std::runtime_error("gltf file index buffer has bad index (exceeds length of vertex array)");
	}

	switch(mode)
	{
		case Mode::Triangles:
		{
			This->indices.resize(This->indices.size() + count);
			auto begin = This->indices.data() + start;

			for(uint32_t i = 0; i < count; ++i)
			{
				begin[i] = indices[i] + vertexOffset;
			}
		}
		break;
	case Mode::TriangleStrip:
	{
		This->indices.resize(This->indices.size() + count-2);
		auto begin = This->indices.data() + start;

		for(uint32_t i = 0; i < count-2; ++i)
		{
			begin[i*3+0] = indices[i] + vertexOffset;
			begin[i*3+1] = indices[i+1] + vertexOffset;
			begin[i*3+2] = indices[i+2] + vertexOffset;
		}
	}
	break;
	case Mode::TriangleFan:
	{
		This->indices.resize(This->indices.size() + (count-1)*3);
		auto begin = This->indices.data() + start;

		for(uint32_t i = 1; i < count; ++i)
		{
			begin[i*3+0] = *indices + vertexOffset;
			begin[i*3+1] = indices[i] + vertexOffset;
			begin[i*3+2] = indices[1 + (i+1) % 3] + vertexOffset;
		}
	}
	break;
	default:
		break;
	}
}

static void EvaluateIndices(gltfFile * This, fx::gltf::Document & doc, int indices, int vertexOffset, size_t noVertices, Mode mode)
{
	if((uint32_t)indices > doc.accessors.size())
	{
		std::vector<uint32_t> indices(noVertices);

		for(auto i = 0u; i < indices.size(); ++i)
			indices[i] = i;

		return EvaluateIndices(This, indices.data(), indices.size(), indices.size() * sizeof(indices[0]), vertexOffset, noVertices, mode);
	}

	auto const& accessor = doc.accessors[indices];

	if((uint32_t)accessor.bufferView > doc.bufferViews.size())
		throw std::runtime_error("gltf file has bad bufferView index in accessor");

	if(accessor.type != Type::None && accessor.type != Type::Scalar)
		throw std::runtime_error("gltf file has incorrect accessor type in index buffer");

	auto const& bufferView = doc.bufferViews[accessor.bufferView];

	if((uint32_t)bufferView.buffer > doc.buffers.size())
		throw std::runtime_error("gltf file has bad buffer index in bufferView");

	if(bufferView.target != TargetType::None && bufferView.target != TargetType::ElementArrayBuffer)
		throw std::runtime_error("gltf file index buffer bufferView has bad target type");

	if(bufferView.byteStride != 0)
		throw std::runtime_error("gltf file index buffer bufferView has byte stride");

	auto const& buffer = doc.buffers[bufferView.buffer];

	if(buffer.data.size() < bufferView.byteOffset + bufferView.byteLength + accessor.byteOffset)
		throw std::runtime_error("gltf file bufferView has bad offset");

	void const* ptr = &buffer.data[bufferView.byteOffset + accessor.byteOffset];

	switch(accessor.componentType)
	{
	case ComponentType::Byte:
		EvaluateIndices(This, (int8_t const*)ptr, accessor.count, bufferView.byteLength, vertexOffset, noVertices, mode);
		return;
	case ComponentType::UnsignedByte:
		EvaluateIndices(This, (uint8_t const*)ptr, accessor.count, bufferView.byteLength, vertexOffset, noVertices, mode);
		return;
	case ComponentType::Short:
		EvaluateIndices(This, (int16_t const*)ptr, accessor.count, bufferView.byteLength, vertexOffset, noVertices, mode);
		return;
	case ComponentType::UnsignedShort:
		EvaluateIndices(This, (uint16_t const*)ptr, accessor.count, bufferView.byteLength, vertexOffset, noVertices, mode);
		return;
	case ComponentType::Int:
		EvaluateIndices(This, (int32_t const*)ptr, accessor.count, bufferView.byteLength, vertexOffset, noVertices, mode);
		return;
	case ComponentType::UnsignedInt:
		EvaluateIndices(This, (uint32_t const*)ptr, accessor.count, bufferView.byteLength, vertexOffset, noVertices, mode);
		return;
	default:
		throw std::runtime_error("gltf file index buffer accessor has invalid component type");
	}

}

void ThrowZlib(int code);

template<typename T>
void PushData(std::vector<uint8_t> & r, std::vector<T> const& vec)
{
	size_t pos = r.size();
	r.resize(r.size() + sizeof(T) * vec.size());
	memcpy(&r[pos], vec.data(), sizeof(T) * vec.size());
}

template<typename T>
void PushData(std::vector<uint8_t> & r, T t)
{
	static_assert(std::is_fundamental<T>::value);

	uint32_t padding = sizeof(T) - (r.size() % sizeof(T));

	if(padding < sizeof(T))
	{
		r.resize(r.size() + (padding), 0);
	}

	size_t pos = r.size();
	r.resize(r.size() + sizeof(T));
	memcpy(&r[pos], &t, sizeof(T));
}

template<typename T, typename...Args>
void PushData(std::vector<uint8_t> & r, T t, Args const&...args)
{
	PushData(r, t);
	PushData(r, args...);
}

template<typename...Args>
std::vector<uint8_t> MakeByteVector(Args const&...args)
{
	std::vector<uint8_t> r;
	PushData(r, args...);
	return r;
}

void gltfFile::Compress()
{
	if(vertices.empty())
		return;

	if(vertices.size() >= USHRT_MAX)
		compressed = MakeByteVector(
			(uint32_t)fx::gltf::BufferView::TargetType::ArrayBuffer,
			(uint32_t)Type::Vec3,
			(uint32_t)ComponentType::Float,
			(uint32_t)vertices.size(), vertices,

			(uint32_t)fx::gltf::BufferView::TargetType::ElementArrayBuffer,
			(uint32_t)ComponentType::UnsignedInt,
			(uint32_t)indices.size(),
			indices);
	else
	{
		std::vector<uint16_t> short_indices(indices.size());

		for(uint32_t i = 0; i < indices.size(); ++i)
		{
			short_indices[i] = indices[i];
		}

		compressed = MakeByteVector(
			(uint32_t)fx::gltf::BufferView::TargetType::ArrayBuffer,
			(uint32_t)Type::Vec3,
			(uint32_t)ComponentType::Float,
			(uint32_t)vertices.size(), vertices,
			(uint32_t)fx::gltf::BufferView::TargetType::ElementArrayBuffer,
			(uint32_t)ComponentType::UnsignedShort,
			(uint32_t)indices.size(), short_indices);
	}

#define UNIT_TEST   0

	std::vector<uint8_t> output_block(LZ4_compressBound(compressed.size()));

	printf("compressing shadow volume...\n");

	int total_out = LZ4_compress_HC(
		(const char*)compressed.data(),
		(char*)output_block.data(),
		compressed.size(),
		output_block.size(),
		LZ4HC_CLEVEL_MAX);

	output_block.resize(total_out);
	auto size = (uint32_t)compressed.size();
	compressed = MakeByteVector((uint32_t)compressed.size(), std::move(output_block));
	assert(*(uint32_t*)&compressed[0] == size);
}
