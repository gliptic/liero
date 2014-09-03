#ifndef LIERO_FILESYSTEM_HPP
#define LIERO_FILESYSTEM_HPP

#include <string>
#include <cstdio>
#include <memory>
#include <map>
#include <gvl/support/platform.hpp>
#include <gvl/io2/stream.hpp>
#include <gvl/zlib/zlib2.h>
#include <gvl/resman/shared_ptr.hpp>
#include "reader.hpp"

std::string changeLeaf(std::string const& path, std::string const& newLeaf);
std::string getRoot(std::string const& path);
std::string getLeaf(std::string const& path);
std::string getBasename(std::string const& path);
std::string getExtension(std::string const& path);
void toUpperCase(std::string& str);
std::string joinPath(std::string const& root, std::string const& leaf);

FILE* tolerantFOpen(std::string const& name, char const* mode);

std::size_t fileLength(FILE* f);
bool create_directories(std::string const& dir);

struct dir_itr_imp
{
	virtual ~dir_itr_imp()
	{
	}

	virtual std::string const& deref() = 0;
	virtual bool inc() = 0;
};

typedef std::unique_ptr<dir_itr_imp> dir_itr_imp_ptr;

struct DirectoryIterator
{
	dir_itr_imp_ptr m_imp;
	
	DirectoryIterator(std::string const& dir);
	DirectoryIterator(dir_itr_imp_ptr&& imp);
	~DirectoryIterator();
	
	operator void*()
	{
		return m_imp.get();
	}
	
	std::string const& operator*() const
	{
		return m_imp->deref();
	}
	
	void operator++()
	{
		if (!m_imp->inc())
			m_imp.reset();
	}
};

struct FsNodeImp : gvl::shared
{
	virtual std::string const& fullPath() = 0;
	virtual DirectoryIterator iter() = 0;
	virtual gvl::shared_ptr<FsNodeImp> go(std::string const& name) = 0;
	virtual ReaderFile read() = 0;
	virtual gvl::source toSource() = 0;
};

struct FsNodeZipFile;

struct FsNodeZipArchive : gvl::shared
{
	FsNodeZipArchive(std::string const& path);

	mz_zip_archive archive;

	FsNodeZipFile* root;
};

struct FsNodeZipFile : FsNodeImp
{
	gvl::shared_ptr<FsNodeZipArchive> archive;
	std::string path;
	std::string relPath;
	int fileIndex;

	FsNodeZipFile(std::string const& path);
	FsNodeZipFile(
		gvl::shared_ptr<FsNodeZipArchive> archive,
		std::string const& path,
		std::string const& relPath,
		int fileIndex);

	std::map<std::string, gvl::shared_ptr<FsNodeZipFile>> children;

	virtual std::string const& fullPath();
	virtual DirectoryIterator iter();
	virtual gvl::shared_ptr<FsNodeImp> go(std::string const& name);
	virtual ReaderFile read();
	virtual gvl::source toSource();
};

struct FsNodeFilesystem : FsNodeImp
{
	FsNodeFilesystem(std::string const& path)
	: path(path)
	{
	}
	
	virtual std::string const& fullPath();
	virtual DirectoryIterator iter();
	virtual gvl::shared_ptr<FsNodeImp> go(std::string const& name);
	virtual ReaderFile read();
	virtual gvl::source toSource();

	std::string path;
};

struct FsNode
{
	gvl::shared_ptr<FsNodeImp> imp;

	FsNode()
	{
	}

	FsNode(std::string const& path);

	FsNode(FsNode&& other)
	: imp(std::move(other.imp))
	{
	}

	FsNode(gvl::shared_ptr<FsNodeImp> imp)
	: imp(std::move(imp))
	{
	}

	FsNode& operator=(FsNode&& other)
	{
		imp = std::move(other.imp);
		return *this;
	}

	operator void*() const
	{
		return imp.get();
	}

	std::string const& fullPath() const
	{ return imp->fullPath(); }

	DirectoryIterator iter() const
	{ return imp->iter(); }

	FsNode operator/(std::string const& name) const
	{ return FsNode(imp->go(name)); }

	ReaderFile read() const
	{ return imp->read(); }

	gvl::octet_reader toOctetReader() const
	{ return gvl::octet_reader(imp->toSource()); }

	gvl::source toSource() const
	{ return imp->toSource(); }
};

#endif // LIERO_FILESYSTEM_HPP
