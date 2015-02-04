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

typedef bool (*FileFilter)(string const& name, string const& ext);

struct FileNode : gvl::shared
{
	FileNode()
	: folder(true)
	, id(0)
	, selectedChild(0)
	, parent(0)
	, filter(0)
	, menu(178, 28)
	, filled(false)
	{
		menu.setHeight(14);
	}

	FileNode(string const& name, string const& pathName, string const& fullPath, bool folder, FileNode* parent, FileFilter filter = 0)
	: name(name), fullPath(fullPath)
	, folder(folder)
	, id(0)
	, selectedChild(0)
	, parent(parent)
	, filter(filter)
	, menu(178, 28)
	, pathName(pathName)
	, filled(false)
	{
		menu.setHeight(14);
	}

	void fill();

	Menu& getMenu()
	{
		ensureFilled();

		if (menu.items.empty())
		{
			for (auto& c : children)
				menu.addItem(MenuItem(c->folder ? 47 : 48, 7, c->name));
	
			menu.moveToFirstVisible();
		}

		return menu;
	}

	FileNode* find(string const& path)
	{
		if (ciCompare(fullPath, path))
			return this;
		if (!ciStartsWith(path, fullPath))
			return 0;
		if (!folder)
			return 0;

		ensureFilled();

		for (auto& c : children)
		{
			auto* r = c->find(path);
			if (r) return r;
		}

		return 0;
	}

	FsNode& getFsNode()
	{
		if (!fsNode)
		{
			assert(parent);
			assert(parent->fsNode);
			fsNode = parent->fsNode / pathName;
		}

		return fsNode;
	}

	void ensureFilled()
	{
		if (!filled && parent)
		{
			getFsNode();
			fill();
		}
	}

	string name;
	string fullPath;
	bool folder;
	int id;
	FileNode* selectedChild;
	FileNode* parent;
	vector<shared_ptr<FileNode> > children;
	bool (*filter)(std::string const& name, std::string const& ext);
	Menu menu;
	
	string pathName;
	bool filled;
	FsNode fsNode; // Starts out invalid
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

void FileNode::fill()
{
	assert(fsNode);
	DirectoryListing di(fsNode.iter());

	for(auto const& name : di)
	{
		string const& fullPath = joinPath(this->fullPath, name.name);
		auto const& ext = getExtension(name.name);

		if (name.isDir)
		{
			shared_ptr<FileNode> node(new FileNode(
				name.name, name.name, fullPath, true, this, filter));

			children.push_back(node);
		}
		else if (!filter || filter(name.name, ext))
		{
			children.push_back(shared_ptr<FileNode>(new FileNode(
				getBasename(name.name), name.name, fullPath, false, this, filter)));
		}
	}

	std::sort(children.begin(), children.end(), ChildSort());

	filled = true;
}

struct FileSelector
{
	FileSelector(Common& common, int x = 178)
	: common(common)
	{
		
	}

	void fill(string const& path, FileFilter filter) // TODO: Get rid of this
	{
		fill(FsNode(path), filter);
	}

	void fill(FsNode node, FileFilter filter)
	{
		rootNode.fsNode = std::move(node);
		rootNode.fullPath = rootNode.fsNode.fullPath();
		rootNode.filter = filter;
		rootNode.fill();
	}

	void draw()
	{
		if (currentNode && currentNode->parent)
		{
			common.font.drawFramedText(gfx.primaryRenderer.bmp, "Parent directory", 28, 20, 50);
			currentNode->parent->getMenu().draw(common, true, 28, true);
		}
		menu().draw(common, false, 178);
	}

	Menu& menu()
	{
		return currentNode->getMenu();
	}

	void setFolder(FileNode& fn)
	{
		currentNode = &fn;
	}

	bool select(string const& path)
	{
		FileNode* fn = rootNode.find(path);

		if (!fn)
			return false;

		FileNode* parent = fn->parent;
		if (parent)
		{
			FileNode* p = fn;
			while (p->parent)
			{
				FileNode* ch = p;
				p = p->parent;
				p->selectedChild = fn;

				for (std::size_t i = 0; i < p->children.size(); ++i)
				{
					if (ch == p->children[i].get())
						p->getMenu().moveTo((int)i);
				}
			}

			if (fn->folder)
				setFolder(*fn);
			else
				setFolder(*parent);
		}

		return true;
	}

	FileNode* curSel()
	{
		if (!menu().isSelectionValid())
			return 0;

		auto* c = currentNode->children[menu().selection()].get();

		return c;
	}

	FileNode* enter()
	{
		auto* c = curSel();
		if (!c)
		{
			return 0;
		}

		if (c->folder)
		{
			currentNode->selectedChild = c;
			setFolder(*c);
			return 0;
		}

		return c;
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
		if(gfx.testSDLKeyOnce(SDL_SCANCODE_UP))
		{
			sfx.play(common, 26);
			
			menu().movement(-1);
		}

		if(gfx.testSDLKeyOnce(SDL_SCANCODE_DOWN))
		{
			sfx.play(common, 25);
			
			menu().movement(1);
		}

		if(gfx.testSDLKeyOnce(SDL_SCANCODE_PAGEUP))
		{
			sfx.play(common, 26);
				
			menu().movementPage(-1);
		}

		if(gfx.testSDLKeyOnce(SDL_SCANCODE_PAGEDOWN))
		{
			sfx.play(common, 25);
				
			menu().movementPage(1);
		}

		if (gfx.testSDLKeyOnce(SDL_SCANCODE_ESCAPE))
		{
			return false;
		}

		if (gfx.testSDLKeyOnce(SDL_SCANCODE_LEFT))
		{
			exit();
		}

		if (gfx.testSDLKeyOnce(SDL_SCANCODE_RIGHT))
		{
			enter();
		}

		menu().onKeys(gfx.keyBuf, gfx.keyBufPtr, true);

		return true;
	}

	Common& common;
	//vector<pair<FileNode*, int> > history;

	FileNode rootNode;
	FileNode* currentNode;

	//Menu menu;
};

#endif // LIERO_MENU_FILESELECTOR_HPP
