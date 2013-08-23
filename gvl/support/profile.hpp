#ifndef UUID_8F2AAEBF84EF4C73603D7DA6D0FD36F1
#define UUID_8F2AAEBF84EF4C73603D7DA6D0FD36F1

#include <cstddef>
#include <ostream>
#include <ctime>

#include "../system/system.hpp"
#include "../support/macros.hpp"

namespace gvl
{

struct profile_counter
{
	profile_counter(char const* desc, char const* func, int line);
	
	void operator++()
	{
		++count;
	}
	
	void operator+=(std::size_t n)
	{
		count += n;
	}
	
	std::size_t count;
	char const* desc;
	char const* func;
	int line;
};

struct profile_timer
{
	profile_timer(char const* desc, char const* func, int line);
		
	uint32_t total_time;
	char const* desc;
	char const* func;
	int line;
	std::size_t count;
};

struct profile_accum_timer
{
	profile_accum_timer(profile_timer& timer_init)
	: timer(timer_init)
	{
		start_time = get_ticks();
	}
	
	~profile_accum_timer()
	{
		uint32_t end_time = get_ticks();
		++timer.count;
		timer.total_time += (end_time - start_time);
	}
	
	uint32_t start_time;
	profile_timer& timer;
};

void present_profile(std::ostream& str);

#if GVL_PROFILE
#define GVL_PROF_COUNT(desc) static gvl::profile_counter GVL_CONCAT(_vl_profcount, __LINE__) ((desc), __FUNCTION__, __LINE__); ++ GVL_CONCAT(_vl_profcount, __LINE__)
#define GVL_PROF_SUM(desc, num) static gvl::profile_counter GVL_CONCAT(_vl_profcount, __LINE__) ((desc), __FUNCTION__, __LINE__); GVL_CONCAT(_vl_profcount, __LINE__) += std::size_t(num);
#define GVL_PROF_TIMER(desc) static gvl::profile_timer GVL_CONCAT(_vl_prof_timer, __LINE__) ((desc), __FUNCTION__, __LINE__); gvl::profile_accum_timer GVL_CONCAT(_vl_prof_accum_timer, __LINE__) (GVL_CONCAT(_vl_prof_timer, __LINE__))
#define GVL_PROF_DECLCOUNT(name, desc) static gvl::profile_counter name((desc), __FUNCTION__, __LINE__)
#define GVL_PROF_DECLTIMER(name, desc) static gvl::profile_timer name((desc), __FUNCTION__, __LINE__)
#else
#define GVL_PROF_COUNT(desc) ((void)0)
#define GVL_PROF_SUM(desc, num) ((void)0)
#define GVL_PROF_TIMER(desc) ((void)0)
#define GVL_PROF_DECLCOUNT(name, desc) ((void)0)
#define GVL_PROF_DECLTIMER(name, desc) ((void)0)
#endif

} // namespace gvl

#endif // UUID_8F2AAEBF84EF4C73603D7DA6D0FD36F1
