#include "blurheightmap.h"
#if 0
#include "depthfile.h"
#include "height_mask.h"
#include "backgroundexception.h"
#include "blur_config.h"
#include "png_file.h"
#include <cstring>
#include <algorithm>
#include <array>

const glm::ivec2 kOffsets[] =
{
{-1, -1}, {0, -1}, {1, -1}, {-1, 0},
{-1,  1}, {0,  1}, {1,  1}, { 1, 0}
};

const uint8_t kCorners[4] = {0, 2, 4, 6 };
const uint8_t kSides[4] = {1, 3, 5, 7};

static std::unique_ptr<glm::u8vec2[]> GetNeighborMask(const float * level, glm::ivec2 size, bool clamp_to_edge, bool itr_2 = false);
static std::unique_ptr<float[]> CopyToFloat(PngFile &m);

static void PngFromFloats(PngFile &, const float * values, glm::ivec2 size);
static void To16BitGrey(PngFile &, PngFile & src);
static std::unique_ptr<float[]> DiffuseStep(PngFile &m, std::unique_ptr<float[]> heights, glm::u8vec2 * color_mask, const std::vector<BlurStage> & stages);

void BlurHeightMap(PngFile & dst, PngFile & src, DepthFile & depth, std::vector<BlurStage> const& stages)
{
	src.Read();

	if(src.width() != depth.size.x
	|| src.height() != depth.size.y)
	{
		throw BackgroundException("Dimensions of '" + src.path + "' do not match that of the platform map.");
	}

	auto height   = CopyToFloat(src);

	if(stages.size())
	{
		auto mask = GetNeighborMask(height.get(), depth.size, false);
		height    = DiffuseStep(src, std::move(height), mask.get(), stages);
	}


	PngFromFloats(dst, height.get(), depth.size);
}

glm::u8vec2 GetMaskValue(float const* array, glm::ivec2 size, int x, int y, bool clamp)
{
	const uint8_t current = array[y*size.x + x];

	glm::u8vec2 value = 0;

	for(int i = 0; i < 8; ++i)
	{
		glm::ivec2 coord = glm::ivec2(x, y) + kOffsets[i];

		if(0 <= coord.x && coord.x < size.x
		&& 0 <= coord.y && coord.y < size.y)
		{
			value |= (((uint8_t)(array[coord.y*size.x + coord.x])) == current) << i;
		}
		else
		{
			value |= clamp << i;
		}
	}

	return value;
}


std::unique_ptr<glm::u8vec2[]> GetNeighborMask(float const* level, glm::ivec2 size, bool clamp_to_edge, bool itr_2)
{
	std::unique_ptr<glm::u8vec2[]> r(new glm::u8vec2[size.x*size.y]);

//copy platform
	for(int y = 0; y < size.y; ++y)
	{
		for(int x = 0; x < size.x; ++x)
		{
			r[y*size.x + x] = GetMaskValue(level, size, x, y, clamp_to_edge);
		}
	}


//if a nearby pixel has a different G value than us;
//then we can't use it for sampling.
/*
	for(uint32_t y = 0; y < height; ++y)
	{
		for(uint32_t x = 0; x < width; ++x)
		{
			auto mask = r[y*width+x];

			uint8_t g_value = 0;

			for(int i = 0; i < 8; ++i)
			{
				glm::ivec2 coord = glm::ivec2(x, y) + kOffsets[i];

				if(0 <= coord.x && (uint32_t)coord.x < width
				&& 0 <= coord.y && (uint32_t)coord.y < height)
				{
					g_value |= (r[coord.y*width+coord.x].g == 0xFF) << i;
				}
			}

			r[y*width+x].r &= g_value;
		}
	}*/

	if(itr_2)
	{
		for(int y = 0; y < size.y; ++y)
		{
			for(int x = 0; x < size.x; ++x)
			{
				uint8_t r_value = 0;

				for(int i = 0; i < 8; ++i)
				{
					glm::ivec2 coord = glm::ivec2(x, y) + kOffsets[i];

					if(0 <= coord.x && coord.x < size.y
					&& 0 <= coord.y && coord.y < size.x)
					{
						r_value |= (r[coord.y*size.x+coord.x].r == 0xFF) << i;
					}
				}

				r[y*size.x+x].g = r_value;
			}
		}
	}

	return r;
}

void To16BitGrey(PngFile & r, PngFile & src)
{
	r.size     = src.size;
	r.color_type = PngFile::ColorType::GRAY;
	r.bit_depth  = 16;
	r.channels   = 1;
	r.bytesPerRow = 2 * src.width();

	r.Alloc();

	int channel = (src.channels >= 3);

	for(int y = 0; y < src.height(); ++y)
	{
		auto input =  src.row_pointers[y];
		auto output = (uint16_t*) (r.row_pointers[y]);

		for(int x = 0; x < src.width(); ++x)
		{
			output[x] = input[x * src.channels + channel];
		}
	}
}

static std::unique_ptr<float[]> CopyToFloat(PngFile &m)
{
	if(m.bit_depth != 8)
	{
		throw BackgroundException("file: '" + m.path + "' must be an 8 bit image!");
	}

	const uint32_t width = m.width();
	const uint32_t height = m.height();

	std::unique_ptr<float[]> r(new float[width*height]);

	int channel = (m.channels >= 3);

	for(uint32_t y = 0; y < height; ++y)
	{
		for(int x = 0; x < m.width(); ++x)
		{
			r[y*width+x] = m.row_pointers[y][x * m.channels + channel];
		}
	}

	return r;

}

void PngFromFloats(PngFile & r, float const* values, glm::ivec2 size)
{
	r.size     = size;
	r.color_type = PngFile::ColorType::GRAY;
	r.bit_depth  = 16;
	r.channels   = 1;
	r.bytesPerRow = 2 * size.x;

	r.Alloc();

	uint16_t * dst = (uint16_t*) r.row_pointers[0];

	uint32_t N = size.x * size.y;
	for(uint32_t i = 0; i < N; ++i)
	{
		dst[i] = std::max(0, std::min<int>(USHRT_MAX, values[i] * 0xFF));
		dst[i] = (dst[i] << 8) | (dst[i] >> 8);
	}
}

class DiffuseStepLogic;

struct DiffuseInfo : BlurStage
{
	DiffuseStepLogic * _this;

	float * volatile* dst_ptr;
	float * volatile* src_ptr;
	PngFile * png;
	glm::u8vec2 * color_mask;
	glm::u8vec2 * min_max;
	bool is_running;

	glm::u8vec2 * row_min_max;
};

#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>


class DiffuseStepLogic
{
	std::mutex              mutex;
	std::condition_variable condition;
	std::condition_variable wait_condition;
	std::atomic<int>        atomic_y{USHRT_MAX+1};
	std::atomic<int>        thread_count{};
	std::atomic<int>        wait_count{};

public:
	void Step(DiffuseInfo * in, bool is_main)
	{
		++thread_count;
		const int width = in->png->width();
		const int height = in->png->height();
		int channel = (in->png->channels > 2);

		if(is_main && in->is_running == true)
		{
			std::unique_lock<std::mutex> lock(mutex);
			atomic_y = 0;
			condition.notify_all();
		}
		else if(in->is_running == false)
		{
			std::unique_lock<std::mutex> lock(mutex);
			condition.notify_all();
			return;
		}

		float a = .5f;

		while(in->is_running)
		{
			int y;

			float * src = *in->src_ptr;
			float * dst = *in->dst_ptr;

			while((y = atomic_y++) < height)
			{
			//	auto min_max = in->row_min_max[y];

				if(in->color >= 0
				&& !(in->row_min_max[y][0] <= in->color && in->color <= in->row_min_max[y][1]))
					continue;

				for(int x = 0; x < width; ++x)
				{
					if(in->color >= 0)
						continue;

					const float current = src[y* width+x];
					const uint8_t q = in->png->row_pointers[y][x * in->png->channels + channel];

				//	if(current == -1)	continue;

				//	if(x == in->row_info[y][1])	x = in->row_info[y][2];

					const auto mask = in->color_mask[y* width+x];

					glm::i16vec2 norm(q, q);

					float eax = 0;
					int count = 0;

//don't bound min/max if we're in a flat area
					float   min = 255;
					float   max = 0;

//if we're in a sloped area get our actual slopes...
					if((mask.r | ~mask.g) != 0xFF)
					{
						min = q;
						max = q;
						norm = in->min_max[y*width+x];
					}

					for(int i = 0; i < 8; ++i)
					{
	//really can't handle cleaning here; the process would need a few iterations...
						auto c = glm::ivec2(x, y) + kOffsets[i];
						int level = 1 + (i % 2);
						count += level;

	//it is part of a different object
						if((mask.g & (1 << i)) == false
	//clamp to edge
						|| !(0 <= c.x && c.x < width
						&&   0 <= c.y && c.y < height))
						{
							eax += q * level;
							continue;
						}

						float value = src[c.y* width+c.x];

//this is here to enforce sharp edges?
						if(!(q - in->min_delta <= value && value <= q + in->max_delta))
						{
							eax += q * level;
							continue;
						}

						int p = in->png->row_pointers[c.y][c.x * in->png->channels + channel];
						auto it_norm = in->min_max[c.y*width+c.x];

		//it is greater than us
						if(p > q)
						{
	//get our slope
							float slope_a = (q - norm.x);
	//get slope from it to us
							float slope_b = (p - q);
	//get it's max slope
							float slope_c = (it_norm.y - p);

							float diff = slope_b - (slope_c - slope_a)*in->diff_power;

							diff = (in->diff_factor + diff)*in->diff_mul;
							diff = std::min(diff, in->max_slope);
							diff *= (diff > 0);

							if(value > q + diff)
								value = q;
						}
						else if(p < q)
						{
	//get max slope
							float slope_a = (norm.y - q);
	//get slope from it to us
							float slope_b = (q - p);
	//get it's min slope
							float slope_c = (p - it_norm.x);

							float diff = slope_b - (slope_c - slope_a)*in->diff_power;

							diff = (in->diff_factor + diff)*in->diff_mul;
							diff = std::min(diff, in->max_slope);
							diff *= (diff > 0);

							if(value < q - diff)
								value = q;
						}

				//		value = std::max(q-8.f, std::min(q+8.f, value));

						eax += value * level;
					}

					if(in->clamp && min < max)
						eax = std::max<float>(min*count, std::min<float>(max*count, eax));

					dst[y*width+x] = (current + a * eax) / (1 + a * count);

				//	dst[y*m->width+x] = std::max<float>(min, std::min<float>(max, dst[y*m->width+x]));
				}
			}

			if(is_main || in->is_running == false)
				break;

			std::unique_lock<std::mutex> lock(mutex);
			if(thread_count == ++wait_count)
			{
				wait_condition.notify_all();
			}

			while(atomic_y > height && in->is_running)
			{
				condition.wait(lock);
			}
			--wait_count;
		}

		--thread_count;

		if(is_main)
		{
			std::unique_lock<std::mutex> lock(mutex);
			while(thread_count != wait_count)
			{
				wait_condition.wait(lock);
			}
		}
	}
};

void DiffuseStepInternal(DiffuseInfo * in, bool is_main)
{
	in->_this->Step(in, is_main);
}

static std::unique_ptr<glm::u8vec2[]> GetMinMax(PngFile & m)
{
	std::unique_ptr<glm::u8vec2[]> v(new glm::u8vec2[m.width()*m.height()]);

	for(int y = 0; y < m.height(); ++y)
	{
		for(int x = 0; x < m.width(); ++x)
		{
			auto red    = m.row_pointers[y][x*m.channels+0];

			glm::u8vec2 it(255, 0);

			for(int i = -1; i <= 1; ++i)
			{
				for(int j = -1; j <= 1; ++j)
				{
					if(0 <= x+i && x+1 < m.width()
					&& 0 <= y+j && y+j < m.height()
					&& red == m.row_pointers[y+j][(x+i)*m.channels+0])
					{
						it.x = std::min(it.x, m.row_pointers[y+j][(x+i)*m.channels+1]);
						it.y = std::max(it.y, m.row_pointers[y+j][(x+i)*m.channels+1]);
					}
				}
			}

			v[y*m.width()+x] = it;
		}
	}

	return v;
}

static void CommitPng(PngFile & m, float * heights)
{
	int channel = (m.channels >= 3);

	for(int y = 0; y < m.height(); ++y)
	{
		for(int x = 0; x < m.width(); ++x)
		{
			m.row_pointers[y][x*m.channels+channel] = heights[y*m.width() + x];
		}
	}
}

std::unique_ptr<float[]> DiffuseStep(PngFile &m, std::unique_ptr<float[]> heights, glm::u8vec2 * color_mask, std::vector<BlurStage> const & stages)
{

	std::unique_ptr<float[]> delta(new float[m.width()*m.height()]);
	auto min_max = GetMinMax(m);
	auto row_min_max = GetRowMinMax(m);

	std::atomic<int> y{};
	std::array<std::unique_ptr<std::thread>, 4> threads;

	float * src = heights.get();
	float * dst = delta.get();

	DiffuseInfo info;
	DiffuseStepLogic logic;

	info._this   = &logic;
	info.dst_ptr = &dst;
	info.src_ptr = &src;
	info.png     = &m;
	info.color_mask = color_mask;
	info.is_running = true;
	info.min_max = min_max.get();

	info.row_min_max = row_min_max.get();

	for(size_t i = 0; i < threads.size(); ++i)
	{
		threads[i].reset(new std::thread(DiffuseStepInternal, &info, false));
	}

	memcpy(delta.get(), heights.get(), m.width()*m.height() * sizeof(float));

	for(auto const& stage : stages)
	{
		(BlurStage&)info = stage;

		for(int i = 0; i < stage.cycles; ++i)
		{
			DiffuseStepInternal(&info, true);
			std::swap(src, dst);
		}

		if(stage.commit)
		{
			CommitPng(m, src);
		}
	}

	info.is_running = false;
	DiffuseStepInternal(&info, true);

	for(size_t i = 0; i < threads.size(); ++i)
	{
		if(threads[i])
			threads[i]->join();
	}

	if(src == delta.get())
	{
		std::swap(heights, delta);
	}

	return heights;
}

#endif
