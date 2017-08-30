/* ReaPack: Package manager for REAPER
 * Copyright (C) 2015-2017  Christian Fillion
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "index.hpp"

#include "errors.hpp"
#include "filesystem.hpp"
#include "path.hpp"
#include "remote.hpp"

#include <WDL/tinyxml/tinyxml.h>

using namespace std;

Path Index::pathFor(const String &name)
{
  return Path::CACHE + (name + L(".xml"));
}

IndexPtr Index::load(const String &name, const char *data)
{
  TiXmlDocument doc;

  if(data)
    doc.Parse(data);
  else {
    FILE *file = FS::open(pathFor(name));

    if(!file)
      throw reapack_error(FS::lastError());

    doc.LoadFile(file);
    fclose(file);
  }

  if(doc.ErrorId()) {
    const char *err = doc.ErrorDesc();
    throw reapack_error(err);
  }

  TiXmlHandle docHandle(&doc);
  TiXmlElement *root = doc.RootElement();

  if(strcmp(root->Value(), "index"))
    throw reapack_error(L("invalid index"));

  int version = 0;
  root->Attribute("version", &version);

  if(!version)
    throw reapack_error(L("index version not found"));

  Index *ri = new Index(name);

  // ensure the memory is released if an exception is
  // thrown during the loading process
  unique_ptr<Index> ptr(ri);

  switch(version) {
  case 1:
    loadV1(root, ri);
    break;
  default:
    throw reapack_error(L("index version is unsupported"));
  }

  ptr.release();
  return IndexPtr(ri);
}

Index::Index(const String &name)
  : m_name(name)
{
}

Index::~Index()
{
  for(const Category *cat : m_categories)
    delete cat;
}

void Index::setName(const String &newName)
{
  if(!m_name.empty())
    throw reapack_error(L("index name is already set"));

  // validation is taken care of later by Remote's constructor
  m_name = newName;
}

bool Index::addCategory(const Category *cat)
{
  if(cat->index() != this)
    throw reapack_error(L("category belongs to another index"));

  if(cat->packages().empty())
    return false;

  m_catMap.insert({cat->name(), m_categories.size()});
  m_categories.push_back(cat);

  m_packages.insert(m_packages.end(),
    cat->packages().begin(), cat->packages().end());

  return true;
}

const Category *Index::category(const String &name) const
{
  const auto &it = m_catMap.find(name);

  if(it == m_catMap.end())
    return nullptr;
  else
    return category(it->second);
}

const Package *Index::find(const String &catName, const String &pkgName) const
{
  if(const Category *cat = category(catName))
    return cat->package(pkgName);
  else
    return nullptr;
}

Category::Category(const String &name, const Index *ri)
  : m_index(ri), m_name(name)
{
  if(m_name.empty())
    throw reapack_error(L("empty category name"));
}

Category::~Category()
{
  for(const Package *pack : m_packages)
    delete pack;
}

String Category::fullName() const
{
  if(!m_index)
    return m_name;

  String out = m_index->name();
  out += L'/';
  out += m_name;
  return out;
}

bool Category::addPackage(const Package *pkg)
{
  if(pkg->category() != this)
    throw reapack_error(L("package belongs to another category"));

  if(pkg->type() == Package::UnknownType || pkg->versions().empty())
    return false; // silently discard unknown package types

  m_pkgMap.insert({pkg->name(), m_packages.size()});
  m_packages.push_back(pkg);
  return true;
}

const Package *Category::package(const String &name) const
{
  const auto &it = m_pkgMap.find(name);

  if(it == m_pkgMap.end())
    return nullptr;
  else
    return package(it->second);
}
