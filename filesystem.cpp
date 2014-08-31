#include "filesystem.hpp"
#include "text.hpp"
#include <gvl/support/platform.hpp>
#include <gvl/io2/fstream.hpp>
#include <stdexcept>
#include <cassert>
#include <cctype>

std::string changeLeaf(std::string const& path, std::string const& newLeaf)
{
	std::size_t lastSep = path.find_last_of("\\/");
	
	if(lastSep == std::string::npos)
		return newLeaf; // We assume there's only a leaf in the path
	return path.substr(0, lastSep + 1) + newLeaf;
}

std::string getRoot(std::string const& path)
{
	std::size_t lastSep = path.find_last_of("\\/");
	
	if(lastSep == std::string::npos)
		return "";
	return path.substr(0, lastSep);
}

std::string getLeaf(std::string const& path)
{
	std::size_t lastSep = path.find_last_of("\\/");
	
	if(lastSep == std::string::npos)
		return path;
	return path.substr(lastSep + 1);
}

std::string getBasename(std::string const& path)
{
	std::size_t lastSep = path.find_last_of(".");
	
	if(lastSep == std::string::npos)
		return path;
	return path.substr(0, lastSep);
}

std::string getExtension(std::string const& path)
{
	std::size_t lastSep = path.find_last_of(".");
	
	if(lastSep == std::string::npos)
		return "";
	return path.substr(lastSep + 1);
}

void toUpperCase(std::string& str)
{
	for(std::size_t i = 0; i < str.size(); ++i)
	{
		str[i] = std::toupper(static_cast<unsigned char>(str[i])); // TODO: Uppercase conversion that works for the DOS charset
	}
}

void toLowerCase(std::string& str)
{
	for(std::size_t i = 0; i < str.size(); ++i)
	{
		str[i] = std::tolower(static_cast<unsigned char>(str[i])); // TODO: Lowercase conversion that works for the DOS charset
	}
}

FILE* tolerantFOpen(std::string const& name, char const* mode)
{
	FILE* f = std::fopen(name.c_str(), mode);
	if(f)
		return f;
		
	std::string ch(name);
	toUpperCase(ch);
	f = std::fopen(ch.c_str(), mode);
	if(f)
		return f;
		
	toLowerCase(ch);
	f = std::fopen(ch.c_str(), mode);
	if(f)
		return f;
		
	// Try with first letter capital
	ch[0] = std::toupper(static_cast<unsigned char>(ch[0]));
	f = std::fopen(ch.c_str(), mode);
	if(f)
		return f;
		
	return 0;
}

bool fileExists(std::string const& path)
{
	FILE* f = fopen(path.c_str(), "rb");
	bool state = (f != 0);
	if(f) fclose(f);
	return state;
}

std::size_t fileLength(FILE* f)
{
	long old = ftell(f);
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, old, SEEK_SET);
	return len;
}

#if GVL_WINDOWS
#  include "windows.h"

#  if defined(__BORLANDC__) || defined(__MWERKS__)
#     if defined(__BORLANDC__)
        using std::time_t;
#     endif
#     include "utime.h"
#   else
#     include "sys/utime.h"
#   endif
# else
#   include "dirent.h"
#   include "unistd.h"
#   include "fcntl.h"
#   include "utime.h"
#   include <errno.h>
# endif

namespace
{

struct filename_result
{
	filename_result()
	: name(0)
	{
	}
	
	filename_result(char const* name)
	: name(name)
	{
	
	}
	
	operator void const*()
	{
		return name;
	}
	
	char const* name;
};

#if GVL_LINUX || __APPLE__ 

# define BOOST_HANDLE DIR *
# define BOOST_INVALID_HANDLE_VALUE 0
# define BOOST_SYSTEM_DIRECTORY_TYPE struct dirent *

inline filename_result find_first_file( const char * dir,
BOOST_HANDLE & handle, BOOST_SYSTEM_DIRECTORY_TYPE & )
// Returns: 0 if error, otherwise name
{
	const char * dummy_first_name = ".";
	return ( (handle = ::opendir( dir ))
		== BOOST_INVALID_HANDLE_VALUE ) ? filename_result() : filename_result(dummy_first_name);
}  

inline void find_close( BOOST_HANDLE handle )
{
	assert( handle != BOOST_INVALID_HANDLE_VALUE );
	::closedir( handle );
}

inline filename_result find_next_file(
BOOST_HANDLE handle, BOOST_SYSTEM_DIRECTORY_TYPE & )
// Returns: if EOF 0, otherwise name
// Throws: if system reports error
{

	//  TODO: use readdir_r() if available, so code is multi-thread safe.
	//  Fly-in-ointment: not all platforms support readdir_r().

	struct dirent * dp;
	errno = 0;
	if ( (dp = ::readdir( handle )) == 0 )
	{
		if ( errno != 0 )
		{
			throw std::runtime_error("Error iterating directory");
		}
		else { return filename_result(); } // end reached
	}
	return filename_result(dp->d_name);
}
#elif GVL_WINDOWS

# define BOOST_HANDLE HANDLE
# define BOOST_INVALID_HANDLE_VALUE INVALID_HANDLE_VALUE
# define BOOST_SYSTEM_DIRECTORY_TYPE WIN32_FIND_DATAA


inline filename_result find_first_file( const char * dir,
BOOST_HANDLE & handle, BOOST_SYSTEM_DIRECTORY_TYPE & data )
// Returns: 0 if error, otherwise name
{
	//    std::cout << "find_first_file " << dir << std::endl;
	std::string dirpath( std::string(dir) + "/*" );
	bool fail = ( (handle = ::FindFirstFileA( dirpath.c_str(), &data )) == BOOST_INVALID_HANDLE_VALUE );
	
	if(fail)
		return filename_result();
	
	return filename_result(data.cFileName);
}  

inline void find_close( BOOST_HANDLE handle )
{
	//    std::cout << "find_close" << std::endl;
	assert( handle != BOOST_INVALID_HANDLE_VALUE );
	::FindClose( handle );
}

inline filename_result find_next_file(
BOOST_HANDLE handle, BOOST_SYSTEM_DIRECTORY_TYPE & data )
// Returns: 0 if EOF, otherwise name
// Throws: if system reports error
{
	if ( ::FindNextFileA( handle, &data ) == 0 )
	{
		if ( ::GetLastError() != ERROR_NO_MORE_FILES )
		{
			throw std::runtime_error("Error iterating directory");
		}
		else { return filename_result(); } // end reached
	}
	
	return filename_result(data.cFileName);
}

#else

#error "Not supported"
#endif

}

#if GVL_WINDOWS

inline char isDirSep(char c)
{
	return c == '\\' || c == '/';
}

static char const dirSep = '\\';

bool create_directories(std::string const& dir)
{
	for (std::size_t i = 0; i < dir.size(); ++i)
	{
		if (isDirSep(dir[i]))
		{
			std::string path = dir.substr(0, i);
			DWORD attr = GetFileAttributesA(path.c_str());

			if (attr == INVALID_FILE_ATTRIBUTES)
			{
				CreateDirectoryA(path.c_str(), NULL);
			}
			else if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
			{
				return false;
			}
		}
	}

	return true;
}

#else

inline char isDirSep(char c)
{
	return c == '/';
}

static char const dirSep = '/';

#include <sys/types.h>
#include <sys/stat.h>

bool create_directories(std::string const& dir)
{
	for (std::size_t i = 1; i < dir.size(); ++i)
	{
		if (isDirSep(dir[i]))
		{
			std::string path = dir.substr(0, i);
            struct stat s;
            if (stat(path.c_str(), &s) < 0)
            {
                if (mkdir(path.c_str(), 0777) < 0)
                {
                    return false;
                }
            }
		}
	}
    
	return true;
}

#endif

std::string joinPath(std::string const& root, std::string const& leaf)
{
	if(!root.empty()
	&& root[root.size() - 1] != '\\'
	&& root[root.size() - 1] != '/')
	{
		return root + '/' + leaf;
	}
	else
	{
		return root + leaf;
	}
}

void dir_filesystem_itr_imp_init(dir_itr_imp_ptr& m_imp, char const* dir_path);
void dir_zip_archive_itr_imp_init(dir_itr_imp_ptr& m_imp, gvl::shared_ptr<FsNodeZipFile> dir);

DirectoryIterator::DirectoryIterator(std::string const& dir)
{
	dir_filesystem_itr_imp_init(m_imp, dir.empty() ? "." : dir.c_str());
}

DirectoryIterator::DirectoryIterator(dir_itr_imp_ptr&& imp)
: m_imp(std::move(imp))
{
}

DirectoryIterator::~DirectoryIterator()
{
}

FsNodeZipArchive::FsNodeZipArchive(std::string const& path)
{
	memset(&archive, 0, sizeof(archive));
	mz_zip_reader_init_file(&archive, path.c_str(), 0);
}

// TODO: Free archive

FsNodeZipFile::FsNodeZipFile(std::string const& path)
: archive(new FsNodeZipArchive(path))
, path(path)
, fileIndex(-1)
{
	archive->root = this;

	auto fileCount = mz_zip_reader_get_num_files(&archive->archive);
		
	for (mz_uint fileIndex = 0; fileIndex < fileCount; ++fileIndex)
	{
		char buf[260];
		mz_zip_reader_get_filename(&archive->archive, fileIndex, buf, 260);

		std::string filepath(buf);

		FsNodeZipFile* cur = this;

		std::size_t beg = 0;
		std::size_t i = 0;

		for (; i < filepath.size(); ++i)
		{
			if (isDirSep(filepath[i]))
			{
				std::string const& part = filepath.substr(beg, i - beg);
				
				auto& c = cur->children[part];
				if (!c) c.reset(new FsNodeZipFile(archive, joinPath(cur->path, part), joinPath(cur->relPath, part), -1));
				cur = c.get();

				beg = i + 1;
			}
		}

		if (beg != i)
		{
			std::string const& part = filepath.substr(beg, i - beg);
			
			auto& c = cur->children[part];
			if (!c) c.reset(new FsNodeZipFile(archive, joinPath(cur->path, part), joinPath(cur->relPath, part), (int)fileIndex));
			cur = c.get();
		}
	}
}

FsNodeZipFile::FsNodeZipFile(gvl::shared_ptr<FsNodeZipArchive> archive, std::string const& fullPath, std::string const& relPath, int fileIndex)
: archive(std::move(archive))
, path(fullPath)
, relPath(relPath)
, fileIndex(fileIndex)
{
}

std::string const& FsNodeZipFile::fullPath()
{
	return path;
}

DirectoryIterator FsNodeZipFile::iter()
{
	dir_itr_imp_ptr imp;
 	dir_zip_archive_itr_imp_init(imp, gvl::shared_ptr<FsNodeZipFile>(this, gvl::shared_ownership()));
	return DirectoryIterator(std::move(imp));
}

gvl::shared_ptr<FsNodeImp> FsNodeZipFile::go(std::string const& name)
{
	//return std::unique_ptr<FsNodeImp>(new FsNodeZipFile(archive, joinPath(path, name), joinPath(relPath, name)));

	auto i = children.find(name);
	if (i != children.end())
		return i->second;
	return gvl::shared_ptr<FsNodeImp>();
}

ReaderFile FsNodeZipFile::read()
{
	if (fileIndex < 0)
		throw std::runtime_error("Could not find '" + relPath + "\' in zip file");

	std::size_t size;
	auto* ptr = mz_zip_reader_extract_to_heap(&archive->archive, (mz_uint)fileIndex, &size, 0);

	if (!ptr)
		throw std::runtime_error("Could not open '" + relPath + "\' in zip file");

	ReaderFile rf;
	rf.data = (uint8_t*)ptr;
	rf.len = size;
	return std::move(rf);
}

gvl::source FsNodeZipFile::toSource()
{
	if (fileIndex < 0)
		throw std::runtime_error("Could not find '" + relPath + "\' in zip file");

	std::size_t size;
	auto* ptr = mz_zip_reader_extract_to_heap(&archive->archive, (mz_uint)fileIndex, &size, 0);

	if (!ptr)
		throw std::runtime_error("Could not open '" + relPath + "\' in zip file");

	gvl::source s(gvl::make_shared(new gvl::stream_piece(
		gvl::make_shared(gvl::bucket_data_mem::create_from((uint8_t const*)ptr, (uint8_t const*)ptr + size, size)))));
	free(ptr);
	
	return std::move(s);
}

std::string const& FsNodeFilesystem::fullPath()
{
	return path;
}

DirectoryIterator FsNodeFilesystem::iter()
{
	return DirectoryIterator(path);
}

gvl::shared_ptr<FsNodeImp> FsNodeFilesystem::go(std::string const& name)
{
	if (name.size() > 4 && name.find(".zip") == name.size() - 4)
		return gvl::shared_ptr<FsNodeImp>(new FsNodeZipFile(joinPath(path, name)));
	else
		return gvl::shared_ptr<FsNodeImp>(new FsNodeFilesystem(joinPath(path, name)));
}

ReaderFile FsNodeFilesystem::read()
{
	FILE* f = tolerantFOpen(path.c_str(), "rb");
	if(!f)
		throw std::runtime_error("Could not open '" + path + '\'');
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	ReaderFile rf;
	rf.data = (uint8_t*)std::malloc(len);
	rf.len = len;
	fread(rf.data, 1, len, f);
	fclose(f);
	return std::move(rf);
}

gvl::source FsNodeFilesystem::toSource()
{
	FILE* f = tolerantFOpen(path.c_str(), "rb");
	if(!f)
		throw std::runtime_error("Could not open '" + path + '\'');

	return gvl::to_source(new gvl::file_bucket_pipe(f));
}

FsNode::FsNode(std::string const& path)
{
	if (path.empty())
	{
		imp.reset(new FsNodeFilesystem(path));
		return;
	}

	std::size_t beg = 0;
	std::size_t i = 0;

	for (; i < path.size(); ++i)
	{
		if (isDirSep(path[i]))
		{
			std::string const& part = path.substr(beg, i - beg);
			if (!imp)
				imp.reset(new FsNodeFilesystem(part));
			else
				imp = imp->go(part);
			beg = i + 1;
		}
	}

	if (beg != i)
	{
		std::string const& part = path.substr(beg, i - beg);
		if (!imp)
			imp.reset(new FsNodeFilesystem(part));
		else
			imp = imp->go(part);
	}
}

inline bool dot_or_dot_dot( char const * name )
{
	return name[0]=='.'
		&& (name[1]=='\0' || (name[1]=='.' && name[2]=='\0'));
}

//  directory_iterator implementation  ---------------------------------------//

struct dir_filesystem_itr_imp : dir_itr_imp
{
	dir_filesystem_itr_imp()
	{
	}
	
	std::string       entry_path;
	BOOST_HANDLE      handle;

	std::string const& deref();
	bool inc();

	~dir_filesystem_itr_imp()
	{
		if ( handle != BOOST_INVALID_HANDLE_VALUE )
			find_close( handle );
	}
};

void dir_filesystem_itr_imp_init(dir_itr_imp_ptr& m_imp, char const* dir_path)
{
	std::unique_ptr<dir_filesystem_itr_imp> imp(new dir_filesystem_itr_imp);
	
	BOOST_SYSTEM_DIRECTORY_TYPE scratch;
	filename_result name;  // initialization quiets compiler warnings

	if ( !dir_path[0] )
		imp->handle = BOOST_INVALID_HANDLE_VALUE;
	else
		name = find_first_file( dir_path, imp->handle, scratch );  // sets handle

	if ( imp->handle != BOOST_INVALID_HANDLE_VALUE )
	{
		if ( !dot_or_dot_dot( name.name ) )
		{ 
			imp->entry_path = name.name;
		}
		else
		{
			if (!imp->inc())
				return;
		}

		m_imp.reset(imp.release());
	}
}

std::string const& dir_filesystem_itr_imp::deref()
{
	return entry_path;
}

bool dir_filesystem_itr_imp::inc()
{
	assert( handle != BOOST_INVALID_HANDLE_VALUE ); // reality check

	BOOST_SYSTEM_DIRECTORY_TYPE scratch;
	filename_result name;

	while ( (name = find_next_file( handle, scratch )) )
	{
		// append name, except ignore "." or ".."
		if ( !dot_or_dot_dot( name.name ) )
		{
			entry_path = name.name;
			return true;
		}
	}
	
	return false;
}

// zip archive iterator

struct dir_zip_archive_itr_imp : dir_itr_imp
{
	dir_zip_archive_itr_imp(gvl::shared_ptr<FsNodeZipFile> dirInit)
	: dir(std::move(dirInit))
	{
		cur = dir->children.begin();
		end = dir->children.end();
	}
	
	gvl::shared_ptr<FsNodeZipFile> dir;
	std::map<std::string, gvl::shared_ptr<FsNodeZipFile>>::iterator cur, end;

	std::string const& deref();
	bool inc();

	~dir_zip_archive_itr_imp()
	{
	}
};

void dir_zip_archive_itr_imp_init(dir_itr_imp_ptr& m_imp, gvl::shared_ptr<FsNodeZipFile> dir)
{
	std::unique_ptr<dir_zip_archive_itr_imp> imp(new dir_zip_archive_itr_imp(std::move(dir)));

	if (imp->cur != imp->end)
		m_imp.reset(imp.release());
}

std::string const& dir_zip_archive_itr_imp::deref()
{
	return cur->first;
}

bool dir_zip_archive_itr_imp::inc()
{
	return (cur != end && ++cur != end);
}