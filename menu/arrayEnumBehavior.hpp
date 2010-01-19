#ifndef LIERO_MENU_ARRAY_ENUM_BEHAVIOUR_HPP
#define LIERO_MENU_ARRAY_ENUM_BEHAVIOUR_HPP

#include "enumBehavior.hpp"

#include <string>

struct Common;
struct Menu;

struct ArrayEnumBehavior : EnumBehavior
{
	template<int N>
	ArrayEnumBehavior(Common& common, uint32_t& v, std::string const (&arr)[N], bool brokenEnter = false)
	: EnumBehavior(common, v, 0, N-1, brokenEnter)
	, arr(arr)
	{
	}
		
	void onUpdate(Menu& menu, int item)
	{
		MenuItem& i = menu.items[item];
		i.value = arr[v];
		i.hasValue = true;
	}
	
	std::string const* arr;
};

#endif // LIERO_MENU_ARRAY_ENUM_BEHAVIOUR_HPP