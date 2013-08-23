#ifndef GVL_META_IS_PTR_CONVERTIBLE_HPP
#define GVL_META_IS_PTR_CONVERTIBLE_HPP

#include "meta.hpp"

namespace gvl
{

template <typename From, typename To>
struct is_ptr_convertible_impl
{
    template <typename T> struct checker
    {
        static no_type  _m_check(...);
        static yes_type _m_check(T);
    };

    static From _m_from;
    static bool const value = sizeof( checker<To>::_m_check(_m_from) )
        == sizeof(yes_type);
};

template <typename From, typename To>
struct is_ptr_convertible
{
	static bool const value = is_ptr_convertible_impl<From, To>::value;
};

};

#endif // GVL_META_IS_PTR_CONVERTIBLE_HPP
