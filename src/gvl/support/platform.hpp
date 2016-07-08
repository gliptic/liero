#ifndef UUID_E256FC00B8C44EBE85C8738284C064F6
#define UUID_E256FC00B8C44EBE85C8738284C064F6

#include "platform.h"

#if GVL_CPP0X && GVL_CPP
#include <utility>
#define GVL_MOVE(x) (::std::move(x))
#else
#define GVL_MOVE(x) (x)
#endif

#ifdef __cplusplus

namespace gvl
{

struct noncopyable
{
protected:
	noncopyable() {}
    ~noncopyable() {}
private:
	noncopyable(const noncopyable&);
	noncopyable& operator=(const noncopyable&);
};

}

#endif

#endif // UUID_E256FC00B8C44EBE85C8738284C064F6
