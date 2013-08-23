// Copyright David Abrahams 2002.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef UUID_6513633745E64712FFA04FBF1EB80401
#define UUID_6513633745E64712FFA04FBF1EB80401

# include <typeinfo>
# include <cstring>

#   if defined(__GNUC__)                                                \
    && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1)))   \
    && !defined(__EDG_VERSION__)
#    define BOOST_PYTHON_HAVE_GCC_CP_DEMANGLE
#   endif

namespace gvl
{

// for this compiler at least, cross-shared-library type_info
// comparisons don't work, so use typeid(x).name() instead. It's not
// yet clear what the best default strategy is.
# if (defined(__GNUC__) && __GNUC__ >= 3) \
 || defined(_AIX) \
 || (   defined(__sgi) && defined(__host_mips)) \
 || (defined(linux) && defined(__INTEL_COMPILER) && defined(__ICC))
#  define BOOST_PYTHON_TYPE_ID_NAME
# endif 

// type ids which represent the same information as std::type_info
// (i.e. the top-level reference and cv-qualifiers are stripped), but
// which works across shared libraries.
struct type_info
{
    inline type_info(std::type_info const& = typeid(void));
    
    inline bool operator<(type_info const& rhs) const;
    inline bool operator==(type_info const& rhs) const;

    char const* name() const;
    
 private: // data members
#  ifdef BOOST_PYTHON_TYPE_ID_NAME
    typedef char const* base_id_t;
#  else
    typedef std::type_info const* base_id_t;
#  endif
    
    base_id_t m_base_type;
};

template <class T>
inline type_info type_id()
{
    return type_info(typeid(T));
}

//
inline type_info::type_info(std::type_info const& id)
    : m_base_type(
#  ifdef BOOST_PYTHON_TYPE_ID_NAME
        id.name()
#  else
        &id
#  endif
        )
{
}

inline bool type_info::operator<(type_info const& rhs) const
{
#  ifdef BOOST_PYTHON_TYPE_ID_NAME
    return std::strcmp(m_base_type, rhs.m_base_type) < 0;
#  else
    return m_base_type->before(*rhs.m_base_type) != 0;
#  endif 
}

inline bool type_info::operator==(type_info const& rhs) const
{
#  ifdef BOOST_PYTHON_TYPE_ID_NAME
    return !std::strcmp(m_base_type, rhs.m_base_type);
#  else
    return *m_base_type == *rhs.m_base_type;
#  endif 
}


inline char const* type_info::name() const
{
    char const* raw_name
        = m_base_type
#  ifndef BOOST_PYTHON_TYPE_ID_NAME
          ->name()
#  endif
        ;

    return raw_name;
}

} // namespace gvl

#endif // UUID_6513633745E64712FFA04FBF1EB80401
