#ifndef UUID_380EA8845AF747FA66DCB5B9DA57C409
#define UUID_380EA8845AF747FA66DCB5B9DA57C409

namespace gvl
{

struct archive_check_error : std::runtime_error
{
	archive_check_error(std::string const& msg)
	: std::runtime_error(msg)
	{
	}
};


} // namespace gvl

#endif // UUID_380EA8845AF747FA66DCB5B9DA57C409
