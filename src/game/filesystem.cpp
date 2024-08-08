#include "filesystem.hpp"
#include "text.hpp"
#include <gvl/support/platform.hpp>
#include <gvl/io2/fstream.hpp>
#include <tl/platform.h>
#include <stdexcept>
#include <cassert>
#include <cctype>
#include <sys/stat.h>
#if TL_WINDOWS
#include <io.h>
#else
#include <unistd.h>
#endif

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

	if (lastSep == std::string::npos)
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

std::string toUpperCase(std::string str)
{
	for(std::size_t i = 0; i < str.size(); ++i)
	{
		str[i] = std::toupper(static_cast<unsigned char>(str[i])); // TODO: Uppercase conversion that works for the DOS charset
	}
	return str;
}

std::string toLowerCase(std::string str)
{
	for(std::size_t i = 0; i < str.size(); ++i)
	{
		str[i] = std::tolower(static_cast<unsigned char>(str[i])); // TODO: Lowercase conversion that works for the DOS charset
	}
	return str;
}

FILE* tolerantFOpen(std::string const& name, char const* mode)
{
	FILE* f = std::fopen(name.c_str(), mode);
	if(f)
		return f;

	f = std::fopen(toUpperCase(name).c_str(), mode);
	if(f)
		return f;

	f = std::fopen(toLowerCase(name).c_str(), mode);
	if(f)
		return f;

	// Try with first letter capital
	std::string ch(toLowerCase(name));
	ch[0] = std::toupper(static_cast<unsigned char>(ch[0]));
	f = std::fopen(ch.c_str(), mode);
	if(f)
		return f;

	return 0;
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

static char const dirSep = '/';

inline char isDirSep(char c)
{
	return c == dirSep;
}

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

inline bool dot_or_dot_dot( char const * name )
{
	return name[0]=='.'
		&& (name[1]=='\0' || (name[1]=='.' && name[2]=='\0'));
}

DirectoryListing::DirectoryListing(std::string const& dir)
{
	char const* dir_path = dir.empty() ? "." : dir.c_str();

	BOOST_HANDLE handle;
	BOOST_SYSTEM_DIRECTORY_TYPE scratch;
	filename_result name;  // initialization quiets compiler warnings

	if (!dir_path[0])
		handle = BOOST_INVALID_HANDLE_VALUE;
	else
		name = find_first_file(dir_path, handle, scratch);  // sets handle

	if (handle != BOOST_INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!dot_or_dot_dot(name.name))
			{
				struct stat st;
				if (stat(joinPath(dir, name.name).c_str(), &st) == 0)
				{
					subs.push_back(NodeName(name.name, (st.st_mode & S_IFMT) == S_IFDIR));

					if (endsWith(subs.back().name, ".zip"))
					{
						auto& back = subs.back();
						back.name.erase(subs.back().name.size() - 4);
						back.isDir = true;
					}
				}
			}
		}
		while ((name = find_next_file(handle, scratch)));

		find_close(handle);
	}

	sort();
}

DirectoryListing::DirectoryListing(std::vector<NodeName>&& subsInit)
: subs(std::move(subsInit))
{
	sort();
}

DirectoryListing::~DirectoryListing()
{
}

void DirectoryListing::sort()
{
	std::sort(subs.begin(), subs.end(), [](NodeName const& a, NodeName const& b) { return a.name < b.name; });
	auto i = std::unique(subs.begin(), subs.end(), [](NodeName const& a, NodeName const& b) { return a.name == b.name; });
	subs.erase(i, subs.end());
}

// ------------ FsNodeZipFile


struct FsNodeZipFile;

struct FsNodeZipArchive : gvl::shared
{
	FsNodeZipArchive(std::string const& path);

	mz_zip_archive archive;

	FsNodeZipFile* root;
};

gvl::shared_ptr<FsNodeImp> join(gvl::shared_ptr<FsNodeImp> a, gvl::shared_ptr<FsNodeImp> b);

struct FsNodeJoin : FsNodeImp
{
	gvl::shared_ptr<FsNodeImp> a, b;

	FsNodeJoin(gvl::shared_ptr<FsNodeImp> aInit, gvl::shared_ptr<FsNodeImp> bInit)
	: a(std::move(aInit)), b(std::move(bInit))
	{
	}

	std::string const& fullPath()
	{
		return a->fullPath();
	}

	DirectoryListing iter()
	{
		return a->iter() | b->iter();
	}

	gvl::shared_ptr<FsNodeImp> go(std::string const& name)
	{
		return join(a->go(name), b->go(name));
	}

	bool exists() const
	{
		return a->exists() || b->exists();
	}

	gvl::source tryToSource()
	{
		auto s = a->tryToSource();
		if (s)
			return s;
		return b->tryToSource();
	}

	gvl::sink tryToSink()
	{
		auto s = a->tryToSink();
		if (s)
			return s;
		return b->tryToSink();
	}
};

gvl::shared_ptr<FsNodeImp> join(gvl::shared_ptr<FsNodeImp> a, gvl::shared_ptr<FsNodeImp> b)
{
	if (!b)
		return a;
	if (!a)
		return b;
	return gvl::shared_ptr<FsNodeImp>(new FsNodeJoin(std::move(a), std::move(b)));
}

struct FsNodeZipFile : FsNodeImp
{
	gvl::shared_ptr<FsNodeZipArchive> archive;
	std::string path;
	std::string relPath;
	int fileIndex;
	bool isDir;

	FsNodeZipFile(std::string const& path, bool isDir)
	: archive(new FsNodeZipArchive(path))
	, path(path)
	, fileIndex(-1)
	, isDir(isDir)
	{
		archive->root = this;

		auto fileCount = mz_zip_reader_get_num_files(&archive->archive);

		for (mz_uint fileIndex = 0; fileIndex < fileCount; ++fileIndex)
		{
			char buf[260];
			mz_zip_reader_get_filename(&archive->archive, fileIndex, buf, 260);

			bool isDir = mz_zip_reader_is_file_a_directory(&archive->archive, fileIndex);

			std::string filepath(buf);

			FsNodeZipFile* cur = this;

			std::size_t beg = 0, i = 0;

			for (; i < filepath.size(); ++i)
			{
				if (isDirSep(filepath[i]))
				{
					std::string const& part = filepath.substr(beg, i - beg);

					auto& c = cur->children[part];
					if (!c) c.reset(new FsNodeZipFile(archive, joinPath(cur->path, part), joinPath(cur->relPath, part), -1, true));
					cur = c.get();

					beg = i + 1;
				}
			}

			if (beg != i)
			{
				std::string const& part = filepath.substr(beg, i - beg);

				auto& c = cur->children[part];
				if (!c) c.reset(new FsNodeZipFile(archive, joinPath(cur->path, part), joinPath(cur->relPath, part), (int)fileIndex, isDir));
				cur = c.get();
			}
		}
	}

	FsNodeZipFile(gvl::shared_ptr<FsNodeZipArchive> archive, std::string const& fullPath, std::string const& relPath, int fileIndex, bool isDir)
	: archive(std::move(archive))
	, path(fullPath)
	, relPath(relPath)
	, fileIndex(fileIndex)
	, isDir(isDir)
	{
	}

	std::map<std::string, gvl::shared_ptr<FsNodeZipFile>> children;

	std::string const& fullPath()
	{
		return path;
	}

	DirectoryListing iter()
	{
		//std::unique_ptr<dir_zip_archive_itr_imp> imp(new dir_zip_archive_itr_imp(gvl::shared_ptr<FsNodeZipFile>(this, gvl::shared_ownership())));

		//if (imp->cur == imp->end)
		//	imp.reset();

		std::vector<NodeName> subs;

		for (auto& i : children)
		{
			subs.push_back(NodeName(i.first, i.second->isDir));
		}

		return DirectoryListing(std::move(subs));
	}

	gvl::shared_ptr<FsNodeImp> go(std::string const& name)
	{
		auto i = children.find(name);
		if (i != children.end())
			return i->second;
		return gvl::shared_ptr<FsNodeImp>();
	}

	bool exists() const
	{
		return true;
	}

	gvl::source tryToSource()
	{
		if (fileIndex < 0)
			return gvl::source();

		std::size_t size;
		auto* ptr = mz_zip_reader_extract_to_heap(&archive->archive, (mz_uint)fileIndex, &size, 0);

		if (!ptr)
			return gvl::source();

		gvl::source s(new gvl::stream_piece(
			gvl::make_shared(gvl::bucket_data_mem::create_from((uint8_t const*)ptr, (uint8_t const*)ptr + size, size))));
		free(ptr);

		return s;
	}

	gvl::sink tryToSink()
	{
		return gvl::sink(); // We don't want to write to .zip files
	}
};

struct FsNodeFilesystem : FsNodeImp
{
	FsNodeFilesystem(std::string const& pathInit)
	: path(pathInit)
	{
		if (path.empty())
			path.assign(".");
	}

	std::string const& fullPath()
	{
		return path;
	}

	DirectoryListing iter()
	{
		return DirectoryListing(path);
	}

	gvl::shared_ptr<FsNodeImp> go(std::string const& name)
	{
		std::string fullPath(joinPath(path, name));
		gvl::shared_ptr<FsNodeImp> imp;

		//struct stat st;
		//if (stat(fullPath.c_str(), &st) == 0)
		{
			//if ((st.st_mode & S_IFMT) == S_IFDIR)
			//	dir = true;

			imp.reset(new FsNodeFilesystem(fullPath));
		}

		std::string zipPath(fullPath + ".zip");

		if (access(zipPath.c_str(), 0) != -1)
		{
			// We have a zip file, merge nodes
			imp = join(std::move(imp), gvl::shared_ptr<FsNodeImp>(new FsNodeZipFile(zipPath, true)));
		}
		else if (access(fullPath.c_str(), 0) != -1 && endsWith(fullPath, ".zip"))
		{
			imp = join(std::move(imp), gvl::shared_ptr<FsNodeImp>(new FsNodeZipFile(fullPath, true)));
		}

		return imp;
	}

	bool exists() const
	{
		return access(path.c_str(), 0) != -1;
	}

	gvl::source tryToSource()
	{
		FILE* f = tolerantFOpen(path.c_str(), "rb");
		if (!f)
			return gvl::source();

		return gvl::to_source(new gvl::file_bucket_pipe(f));
	}

	gvl::sink tryToSink()
	{
		FILE* f = fopen(path.c_str(), "wb");
		if (!f)
		{
			// Try to create directories
			create_directories(path);
			f = fopen(path.c_str(), "wb");
			if (!f)
				return gvl::sink();
		}

		return gvl::sink(new gvl::file_bucket_pipe(f));
	}

	std::string path;
};

FsNodeZipArchive::FsNodeZipArchive(std::string const& path)
{
	memset(&archive, 0, sizeof(archive));
	mz_zip_reader_init_file(&archive, path.c_str(), 0);
}

// TODO: Free archive

FsNode::FsNode(std::string const& path)
{
	if (path.empty())
	{
		imp.reset(new FsNodeFilesystem(path));
		return;
	}

	std::size_t beg = 0, i = 0;

	for (; i < path.size(); ++i)
	{
		if (isDirSep(path[i]))
		{
			std::string const& part = path.substr(beg, i - beg);
			if (!imp)
			{
#if TL_WINDOWS
				if (part.size() == 2 && part[1] == ':')
					imp.reset(new FsNodeFilesystem(part));
				else
				{
					imp.reset(new FsNodeFilesystem("."));
					imp = imp->go(part);
				}
#else
				if (part.empty())
					imp.reset(new FsNodeFilesystem(part));
				else
				{
					imp.reset(new FsNodeFilesystem("."));
					imp = imp->go(part);
				}
#endif
			}
			else
			{
				imp = imp->go(part);
			}

			beg = i + 1;
		}
	}

	if (beg != i)
	{
		std::string const& part = path.substr(beg, i - beg);

		if (!imp)
		{
#if TL_WINDOWS
			if (path.size() == 2 && path[1] == ':')
				imp.reset(new FsNodeFilesystem(part));
			else
			{
				imp.reset(new FsNodeFilesystem("."));
				imp = imp->go(part);
			}
#else
			if (path.empty())
				imp.reset(new FsNodeFilesystem(part));
			else
			{
				imp.reset(new FsNodeFilesystem("."));
				imp = imp->go(part);
			}
#endif
		}
		else
		{
			imp = imp->go(part);
		}
	}
}
