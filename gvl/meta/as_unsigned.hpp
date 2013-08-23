#ifndef GVL_AS_UNSIGNED_HPP
#define GVL_AS_UNSIGNED_HPP

namespace gvl
{

template<typename T>
struct as_unsigned
{
};

#define AS_UNSIGNED_MAP(FROM, TO) \
template<> \
struct as_unsigned<FROM> \
{ typedef TO type; }; \
template<> \
struct as_unsigned<TO> \
{ typedef TO type; };

AS_UNSIGNED_MAP(char, unsigned char)
AS_UNSIGNED_MAP(short, unsigned short)
AS_UNSIGNED_MAP(int, unsigned int)
AS_UNSIGNED_MAP(long, unsigned long)
AS_UNSIGNED_MAP(long long, unsigned long long)

}

#endif // GVL_AS_UNSIGNED_HPP
