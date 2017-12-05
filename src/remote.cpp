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

#include "remote.hpp"

#include "about.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "index.hpp"
#include "reapack.hpp"
#include "transaction.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/logic/tribool_io.hpp>
#include <regex>
#include <sstream>

using namespace std;
using boost::tribool;
using boost::logic::indeterminate;

static char DATA_DELIMITER = '|';

static bool validateName(const string &name)
{
  using namespace std::regex_constants;

  // see https://en.wikipedia.org/wiki/Filename#Reserved%5Fcharacters%5Fand%5Fwords
  static const regex validPattern("[^*\\\\:<>?/|\"[:cntrl:]]+");
  static const regex invalidPattern(
    "[\\.\x20].*|.+[\\.\x20]|CLOCK\\$|COM\\d|LPT\\d", icase);

  smatch valid, invalid;
  regex_match(name, valid, validPattern);
  regex_match(name, invalid, invalidPattern);

  return !valid.empty() && invalid.empty();
}

static bool validateUrl(const string &url)
{
  // see http://tools.ietf.org/html/rfc3986#section-2
  static const regex pattern(
    "(?:[a-zA-Z0-9._~:/?#[\\]@!$&'()*+,;=-]|%[a-f0-9]{2})+");

  smatch match;
  regex_match(url, match, pattern);

  return !match.empty();
}

RemotePtr Remote::fromString(const string &data)
{
  istringstream stream(data);

  string name;
  getline(stream, name, DATA_DELIMITER);

  string url;
  getline(stream, url, DATA_DELIMITER);

  string enabled;
  getline(stream, enabled, DATA_DELIMITER);

  string autoInstall;
  getline(stream, autoInstall, DATA_DELIMITER);

  if(!validateName(name) || !validateUrl(url))
    return {};

  RemotePtr remote = make_shared<Remote>(name, url);

  try {
    remote->setEnabled(boost::lexical_cast<bool>(enabled));
  }
  catch(const boost::bad_lexical_cast &) {}

  try {
    remote->setAutoInstall(boost::lexical_cast<tribool>(autoInstall));
  }
  catch(const boost::bad_lexical_cast &) {}

  return remote;
}

Remote::Remote(const string &name, const string &url, const bool enabled, const tribool &autoInstall)
  : m_enabled(enabled), m_autoInstall(autoInstall), m_flags(0)
{
  setName(name);
  setUrl(url);
}

void Remote::setName(const string &name)
{
  if(!validateName(name))
    throw reapack_error("invalid name");

  m_name = name;
}

void Remote::setUrl(const string &url, const bool user)
{
  if(!validateUrl(url))
    throw reapack_error("invalid url");
  else if(test(ProtectedFlag) && url != m_url)
    throw reapack_error("cannot change the URL of a protected repository");
  // else if(user && url != m_url)
  //   autoSynchronize();

  m_url = url;
}

void Remote::setEnabled(const bool enable, const bool user)
{
  // if(user && enable && !m_enabled)
  //   autoSynchronize();

  m_enabled = enable;
}

bool Remote::autoInstall(bool fallback) const
{
  if(indeterminate(m_autoInstall))
    return fallback;
  else
    return m_autoInstall;
}

void Remote::setAutoInstall(const tribool &autoInstall, const bool user)
{
  // if(user && autoInstall && (!m_autoInstall || indeterminate(m_autoInstall)))
  //   autoSynchronize();

  m_autoInstall = autoInstall;
}

string Remote::toString() const
{
  ostringstream out;
  out << m_name << DATA_DELIMITER;
  out << m_url << DATA_DELIMITER;
  out << m_enabled << DATA_DELIMITER;
  out << m_autoInstall;

  return out.str();
}

void Remote::fetchIndex(const function<void (const IndexPtr &)> &cb)
{
  // the index is already loaded!
  if(const IndexPtr &index = this->index()) {
    cb(index);
    return;
  }

  Transaction *tx = g_reapack->setupTransaction();

  if(!tx)
    return;

  tx->fetchIndex(shared_from_this());

  tx->onFinish([=] {
    if(const IndexPtr &index = this->index())
      cb(index);
  });

  tx->runTasks();
}

void Remote::about(const bool focus)
{
  fetchIndex([focus] (const IndexPtr &index) {
    g_reapack->about()->setDelegate(make_shared<AboutIndexDelegate>(index), focus);
  });
}

IndexPtr Remote::loadIndex()
{
  m_index.reset();

  const auto &index = make_shared<Index>(shared_from_this());
  index->load();

  m_index = index;

  return index;
}

// void Remote::autoSynchronize()
// {
//   if(!autoInstall(g_reapack->config()->install.autoInstall))
//     return;
//
//   if(Transaction *tx = g_reapack->transaction())
//     tx->synchronize(shared_from_this());
// }

RemotePtr RemoteList::getByName(const std::string &name) const
{
  const auto it = find_if(m_remotes.begin(), m_remotes.end(),
    [&name](const RemotePtr &r) { return *r == name; });

  if(it == m_remotes.end())
    return nullptr;
  else
    return *it;
}

vector<RemotePtr> RemoteList::getEnabled() const
{
  vector<RemotePtr> list;
  copy_if(m_remotes.begin(), m_remotes.end(), back_inserter(list),
    [](const RemotePtr &remote) { return remote->isEnabled(); });
  return list;
}

bool RemoteList::contains(const RemotePtr &remote) const
{
  const auto &it = find(m_remotes.begin(), m_remotes.end(), remote);
  return it != m_remotes.end();
}

void RemoteList::add(const RemotePtr &remote)
{
  m_remotes.emplace_back(remote);
}

void RemoteList::remove(const RemotePtr &remote)
{
  const auto &it = find(m_remotes.begin(), m_remotes.end(), remote);

  if(it != m_remotes.end())
    m_remotes.erase(it);
}
