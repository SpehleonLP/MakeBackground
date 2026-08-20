#ifndef ALPHA_MASK_H
#define ALPHA_MASK_H
#include <memory>
#include <array>
#include <glm/vec2.hpp>
#include <glm/gtc/type_precision.hpp>

class PngFile;
class DepthFile;

struct Dimensions
{
	short min_x{64};
	short min_y{64};
	short max_x{0};
	short max_y{0};

	bool isFull() const  { return min_x == 0 && min_y == 0 && max_x == 64 && max_y == 64; }

	bool isEmpty() const { return max_x < min_x; }
};

class AlphaFile
{
public:
	AlphaFile() = default;
	~AlphaFile() = default;

	void Load(DepthFile &);
	void PrintAlpha(const char * filename, int mip);

	Dimensions GetDimensions(int tile) const;

	bool empty() const { return m_heap == nullptr; }
	size_t size() const { return m_length; }
	glm::ivec2 pixels() const { return m_size; }

	uint32_t width() const { return m_size.x; }
	uint32_t height() const { return m_size.y; }

	uint16_t GetMask(int mipLayer, int x, int y) const;

private:
	glm::u16vec2                m_size{0,0};
	uint32_t                    m_length{};
	std::array<uint16_t*, 4>    m_mask{};
	std::unique_ptr<uint16_t[]> m_heap{};

};


#endif // ALPHA_MASK_H
