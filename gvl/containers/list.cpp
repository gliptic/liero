#include "list.hpp"

#include <set>

namespace gvl
{

std::size_t list_common::size() const
{
	gvl_list_node const* n = first();
	std::size_t c = 0;
	for(; n != &sentinel_; n = n->next)
		++c;
	return c;
}

void list_common::integrity_check()
{
	if(empty())
		return;
		
	std::set<gvl_list_node*> seen;
		
	gvl_list_node* n = sentinel();
	for(;;)
	{
		passert(seen.find(n) == seen.end(), "No cycles");
		sassert(n->next->prev == n);
		sassert(n->prev->next == n);
		
		seen.insert(n);
		
		n = n->next;
		if(n == sentinel())
			break;
	}
}

}
