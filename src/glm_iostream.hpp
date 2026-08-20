#pragma once

#include <ostream>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/vec2.hpp>

template<int m, class T, glm::qualifier P>
inline std::ostream & operator<< (std::ostream & stream, const glm::vec<m, T, P> & db)
{
	stream << "vec" << m << "(";

	for(int i = 0; i < db.length(); ++i)
	{
		stream << db[i];

		if(i+1 < db.length())
			stream << ", ";
	}

	return stream << ")";
}

template<int m, glm::qualifier P>
inline std::ostream & operator<<(std::ostream & stream, const glm::vec<m, char, P> & db)
{
	return stream << glm::ivec4(db);
}

template<class T, glm::qualifier P>
inline std::ostream & operator<< (std::ostream & stream, const glm::tquat<T, P> & collider)
{
	stream << "quat(" << collider[0] << ", " << collider[1] << ", " << collider[2] << ", " << collider[3] << ")";
	return stream;
}

template<int x, int y, typename T, glm::qualifier P>
inline std::ostream & operator<<(std::ostream & stream, const glm::mat<x, y, T, P> & db)
{
	stream << "mat" << x << "x" << y << "(";

	for(int i = 0; i < db.length(); ++i)
	{
		stream << db[i];

		if(i+1 < db.length())
			stream << ", ";
	}

	return stream << ")";
}
