#ifndef BACKGROUNDEXCEPTION_H
#define BACKGROUNDEXCEPTION_H
#include <exception>
#include <sstream>
#include <string>

template<typename T>
inline void Print(std::stringstream  & buffer, T t)
{
	buffer << t;
}

template<typename T, typename... Args>
inline void Print(std::stringstream  & buffer, T t, Args const&...args)
{
	buffer << t;
	Print(buffer, args...);
}

template<typename... Args>
inline std::string to_string(Args const&... args)
{
	std::stringstream buffer;
	Print(buffer, args...);
	return buffer.str();
}


class ApplicationException : public std::exception
{
public:
	ApplicationException(const char * text) :
		m_content(text) { }

	ApplicationException(std::string const& text) :
		m_content(text) { }

	ApplicationException(std::string && text) :
		m_content(std::move(text)) { }

	~ApplicationException() = default;

	const char* what() const noexcept override {
		return m_content.c_str();
	}

protected:
	std::string m_content{};
};

#define Exception_Blueprint(name, parent) \
class name : public parent \
{\
public: \
	template<typename... Args>\
	name(Args const&...args) : parent(to_string(args...)) { } \
	name(const char * text) : parent(text) { } \
	name(std::string const& text) :	parent(text) { } \
	name(std::string && text) :	parent(std::move(text)) { } \
	~name() = default; \
}

Exception_Blueprint(LibPngException, ApplicationException);
Exception_Blueprint(FileException, ApplicationException);
Exception_Blueprint(BackgroundException, ApplicationException);
Exception_Blueprint(DDSException, ApplicationException);


#endif // BACKGROUNDEXCEPTION_H
