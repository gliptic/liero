#pragma once

#include <string>
#include <cstdio>
#include <memory>
#include <map>
#include <miniz.h>

#include "io/stream.hpp"

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

struct FsNodeImp
{
	virtual ~FsNodeImp() = default;
	virtual std::string const& fullPath() = 0;
	virtual DirectoryListing iter() = 0;
	virtual std::shared_ptr<FsNodeImp> go(std::string const& name) = 0;
	virtual std::unique_ptr<io::Reader> tryToReader() = 0;
	virtual std::unique_ptr<io::Writer> tryToWriter() = 0;
	virtual bool exists() const = 0;
};

struct FsNode
{
	std::shared_ptr<FsNodeImp> imp;

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

	FsNode(std::shared_ptr<FsNodeImp> imp)
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

	std::unique_ptr<io::Reader> toReader() const
	{
		auto r = imp->tryToReader();
		if (!r)
			throw std::runtime_error("Could not read " + fullPath());
		return r;
	}

	std::unique_ptr<io::Writer> toWriter() const
	{
		auto w = imp->tryToWriter();
		if (!w)
			throw std::runtime_error("Could not write " + fullPath());
		return w;
	}
};
