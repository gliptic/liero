#ifndef UUID_3B614A27F8FE4D5D7E0254BE877C9E5F
#define UUID_3B614A27F8FE4D5D7E0254BE877C9E5F

#include "integerBehavior.hpp"

struct Common;
struct Menu;

struct TimeBehavior : IntegerBehavior
{
	TimeBehavior(Common& common, int& v, int min, int max, int step = 1, bool percentage = false)
	: IntegerBehavior(common, v, min, max, step, percentage)
	{
	}
	
	void onUpdate(Menu& menu, int item);
};


#endif // UUID_3B614A27F8FE4D5D7E0254BE877C9E5F
