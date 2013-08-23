#ifndef GVL_IOSTREAM_HPP
#define GVL_IOSTREAM_HPP

#include "encoding.hpp"
#include "fstream.hpp"

namespace gvl
{

inline octet_stream_writer& cout()
{
	static octet_stream_writer sr(shared_ptr<stream>(new fstream(stdout)));
	return sr;
}

inline octet_stream_writer& cerr()
{
	static octet_stream_writer sr(shared_ptr<stream>(new fstream(stderr)));
	return sr;
}

}

#endif // GVL_IOSTREAM_HPP
