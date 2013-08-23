#ifndef LIERO_MENU_FILESELECTOR_HPP
#define LIERO_MENU_FILESELECTOR_HPP

#include <string>
#include <vector>
#include <utility>
#include "text.hpp"
#include "menu.hpp"
#include "common.hpp"
#include <gvl/resman/shared_ptr.hpp>

using std::string;
using std::vector;
using std::pair;
using gvl::shared_ptr;

struct FileNode : gvl::shared
{
	FileNode()
	: folder(true)
	, id(0)
	, selectedChild(0)
	, parent(0)
	{
	}

	FileNode(string const& name, string const& fullPath, bool folder, FileNode* parent)
	: name(name), fullPath(fullPath)
	, folder(folder)
	, id(0)
	, selectedChild(0)
	, parent(parent)
	{
	}

	template<typename Filter>
	void fill(string const& path, Filter filter);

	FileNode* find(string const& path)
	{
		if (ciCompare(fullPath, path))
			return this;

		for (auto& c : children)
		{
			auto* r = c->find(path);
			if (r) return r;
		}

		return 0;
	}

	string name;
	string fullPath;
	bool folder;
	int id;
	FileNode* selectedChild;
	FileNode* parent;
	vector<shared_ptr<FileNode> > children;
};

struct ChildSort
{
	typedef shared_ptr<FileNode> type;

	bool operator()(type const& a, type const& b) const
	{
		if (a->folder == b->folder)
			return ciLess(a->name, b->name);
		return a->folder > b->folder;
	}
};

template<typename Filter>
void FileNode::fill(string const& path, Filter filter)
{
	DirectoryIterator di(path);

	for(; di; ++di)
	{
		string const& name = *di;
		string const& fullPath = joinPath(path, name);
		auto const& ext = getExtension(name);

		if (ext.empty())
		{
			shared_ptr<FileNode> node(new FileNode(
				name, fullPath, true, this));

			node->fill(fullPath, filter);
			if (!node->children.empty())
				children.push_back(node);
		}
		else if (filter(name, ext))
		{
			children.push_back(shared_ptr<FileNode>(new FileNode(
				getBasename(name), getBasename(fullPath), false, this)));
		}
	}

	std::sort(children.begin(), children.end(), ChildSort());
}

struct FileSelector
{
	FileSelector(Common& common, int x = 178)
	: common(common)
	, menu(x, 28)
	{
		menu.setHeight(14);
	}

	template<typename Filter>
	void fill(string const& path, Filter filter)
	{
		rootNode.fill(path, filter);
	}

	void setFolder(FileNode& fn)
	{
		currentNode = &fn;
		menu.clear();

		for (auto& c : fn.children)
			menu.addItem(MenuItem(c->folder ? 47 : 48, 7, c->name));
	
		menu.moveToFirstVisible();

		for (std::size_t i = 0; i < currentNode->children.size(); ++i)
		{
			if (currentNode->selectedChild == currentNode->children[i].get())
				menu.moveTo(i);
		}
	}

	void select(string const& path)
	{
		FileNode* fn = rootNode.find(path);

		if (!fn)
			return;

		FileNode* parent = fn->parent;
		if (parent)
		{
			FileNode* p = fn;
			while (p->parent)
			{
				p->parent->selectedChild = fn;
				p = p->parent;
			}

			if (fn->folder)
				setFolder(*fn);
			else
				setFolder(*parent);
		}
	}

	FileNode* enter()
	{
		if (!menu.isSelectionValid())
			return 0;

		auto& c = currentNode->children[menu.selection()];
		if (c->folder)
		{
			currentNode->selectedChild = c.get();
			setFolder(*c);
			return 0;
		}

		return c.get();
	}

	bool exit()
	{
		if (currentNode->parent == 0)
			return false;

		setFolder(*currentNode->parent);
		
		return true;
	}

	bool process()
	{
		if(gfx.testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(common, 26);
			
			menu.movement(-1);
		}
		
		if(gfx.testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(common, 25);
			
			menu.movement(1);
		}

		if(gfx.testSDLKeyOnce(SDLK_PAGEUP))
		{
			sfx.play(common, 26);
				
			menu.movementPage(-1);
		}
			
		if(gfx.testSDLKeyOnce(SDLK_PAGEDOWN))
		{
			sfx.play(common, 25);
				
			menu.movementPage(1);
		}

		if (gfx.testSDLKeyOnce(SDLK_ESCAPE))
		{
			if (!exit())
				return false;
		}

		menu.onKeys(gfx.keyBuf, gfx.keyBufPtr, true);

		return true;
	}

	Common& common;
	//vector<pair<FileNode*, int> > history;

	FileNode rootNode;
	FileNode* currentNode;

	Menu menu;
};

#endif // LIERO_MENU_FILESELECTOR_HPP
