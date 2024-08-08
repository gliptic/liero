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

std::string changeLeaf(std::string const& path, std::string const& newLeaf);
std::string getRoot(std::string const& path);
std::string getLeaf(std::string const& path);
std::string getBasename(std::string const& path);
std::string getExtension(std::string const& path);
std::string toUpperCase(std::string str);
std::string joinPath(std::string const& root, std::string const& leaf);

FILE* tolerantFOpen(std::string const& name, char const* mode);

std::size_t fileLength(FILE* f);
bool create_directories(std::string const& dir);

struct NodeName
{
	NodeName(std::string nameInit, bool isDirInit)
	: name(std::move(nameInit))
	, isDir(isDirInit)
	{
	}

	std::string name;
	bool isDir;
};

struct DirectoryListing
{
	std::vector<NodeName> subs;

	DirectoryListing(DirectoryListing&& other)
	: subs(std::move(other.subs))
	{
	}

	DirectoryListing& operator=(DirectoryListing&& other)
	{
		subs = std::move(other.subs);
		return *this;
	}

	DirectoryListing(std::string const& dir);
	DirectoryListing(std::vector<NodeName>&& subsInit);
	~DirectoryListing();

	DirectoryListing operator|(DirectoryListing const& other)
	{
		DirectoryListing ret(std::move(subs));

		ret.subs.insert(ret.subs.end(), other.subs.begin(), other.subs.end());
		ret.sort();
		return ret;
	}

	std::vector<NodeName>::iterator begin()
	{
		return subs.begin();
	}

	std::vector<NodeName>::iterator end()
	{
		return subs.end();
	}

	void sort();
};

struct FsNodeImp : gvl::shared
{
	virtual std::string const& fullPath() = 0;
	virtual DirectoryListing iter() = 0;
	virtual gvl::shared_ptr<FsNodeImp> go(std::string const& name) = 0;
	virtual gvl::source tryToSource() = 0;
	virtual gvl::sink tryToSink() = 0;
	virtual bool exists() const = 0;
};

struct FsNode
{
	gvl::shared_ptr<FsNodeImp> imp;

	FsNode()
	{
	}

	explicit FsNode(std::string const& path);

	FsNode(FsNode const& other) = default;
	FsNode& operator=(FsNode const& other) = default;

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

	DirectoryListing iter() const
	{ return imp->iter(); }

	FsNode operator/(std::string const& name) const
	{ return FsNode(imp->go(name)); }

	bool exists() const
	{ return imp && imp->exists(); }

	gvl::octet_reader toOctetReader() const
	{
		return gvl::octet_reader(toSource());
	}

	gvl::source toSource() const
	{
		auto s = imp->tryToSource();
		if (!s)
			throw std::runtime_error("Could not read " + fullPath());
		return s;
	}

	gvl::octet_writer toOctetWriter() const
	{
		return gvl::octet_writer(toSink());
	}

	gvl::sink toSink() const
	{
		auto s = imp->tryToSink();
		if (!s)
			throw std::runtime_error("Could not write " + fullPath());
		return s;
	}
};

#endif // LIERO_FILESYSTEM_HPP
