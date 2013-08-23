#ifndef UUID_AEC40E08A7DF4C00C0570A8BE95879BD
#define UUID_AEC40E08A7DF4C00C0570A8BE95879BD

#include <sstream>
#include "encoding.hpp"

namespace gvl
{

template<typename D, typename T>
inline D& operator<<(basic_text_writer<D>& self_, T const& other)
{
	std::stringstream ss;
	ss << other;
	return (self_ << ss.str());
}

}

#endif // UUID_AEC40E08A7DF4C00C0570A8BE95879BD
