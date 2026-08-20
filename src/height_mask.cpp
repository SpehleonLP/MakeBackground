
#include "height_mask.h"
#include "png_file.h"
#include <vector>
#include <algorithm>

std::unique_ptr<glm::u8vec2[]> GetRowMinMax(PngFile const& in)
{
	std::unique_ptr<glm::u8vec2[]> r(new glm::u8vec2[in.height()]);

	for(int y = 0; y < in.height(); ++y)
	{
		uint8_t min = 255;
		uint8_t max = 0;

		for(int x = 0; x < in.width(); ++x)
		{
			min = std::min(min, in.row_pointers[y][x * in.channels + 0]);
			max = std::max(max, in.row_pointers[y][x * in.channels + 0]);
		}

		r[y] = glm::u8vec2(min, max);
	}

	return r;
}


std::unique_ptr<glm::ivec4[]> GetRowInfo(PngFile & in, uint32_t color)
{
	std::unique_ptr<glm::ivec4[]> r(new glm::ivec4[in.height()]);
//		std::vector<glm::ivec4> r(in.height);
	int count = 0;

	for(int y = 0; y < in.height(); ++y)
	{
		r[y] = glm::ivec4(in.width(), 0, 0, 0);

		for(int x = 0; x < in.width(); ++x)
		{
			if(in.row_pointers[y][x * in.channels + 0] == color)
			{
				r[y][0] = x;
				break;
			}
		}

		if(r[y][0] == in.width())
			continue;

		++count;

		r[y][3] = in.width();
		for(int x = in.width()-1; x >= 0; --x)
		{
			if(in.row_pointers[y][x * in.channels + 0] == color)
			{
				r[y][3] = x+1;
				break;
			}
		}

		for(int x = r[y][0]+1; x < r[y][3]; ++x)
		{
			if(in.row_pointers[y][x * in.channels + 0] != color)
			{
				int begin = x;
				int end   = x;

				for(; x < r[y][3]; ++x)
				{
					if(in.row_pointers[y][x * in.channels + 0] == color)
					{
						end = x;
						break;
					}
				}

				if(end - begin > r[y][2] - r[y][1])
				{
					r[y][1] = begin;
					r[y][2] = end;
				}

				break;
			}
		}
	}

	if(count == 0)
		return nullptr;


	return r;


}

