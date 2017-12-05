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

#ifndef REAPACK_REMOTE_HPP
#define REAPACK_REMOTE_HPP

#include <boost/logic/tribool.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Index;
typedef std::shared_ptr<const Index> IndexPtr;

class Remote;
typedef std::shared_ptr<Remote> RemotePtr;

class Remote : public std::enable_shared_from_this<Remote> {
public:
  enum Flag {
    ProtectedFlag = 1<<0,
  };

  static RemotePtr fromString(const std::string &data);

  Remote(const std::string &name, const std::string &url,
    bool enabled = true, const boost::tribool &autoInstall = boost::logic::indeterminate);
  Remote(const Remote &) = delete;

  std::string toString() const;

  const std::string &name() const { return m_name; }

  void setUrl(const std::string &url, bool user = false);
  const std::string &url() const { return m_url; }

  bool isEnabled() const { return m_enabled; }
  void setEnabled(const bool enabled, bool user = false);

  boost::tribool autoInstall() const { return m_autoInstall; }
  bool autoInstall(bool fallback) const;
  void setAutoInstall(const boost::tribool &autoInstall, bool user = false);

  bool test(Flag f) const { return (m_flags & f) != 0; }
  int flags() const { return m_flags; }
  void setFlags(int f) { m_flags = f; }

  void about(bool focus = true);
  // void autoSynchronize();

  IndexPtr loadIndex();
  void fetchIndex(const std::function<void (const IndexPtr &)> &);
  IndexPtr index() const { return m_index.lock(); }

  bool operator==(const std::string &name) const { return m_name == name; }

private:
  void setName(const std::string &name);

  std::string m_name;
  std::string m_url;
  bool m_enabled;
  boost::tribool m_autoInstall;
  int m_flags;
  std::weak_ptr<const Index> m_index;
};

class RemoteList {
public:
  RemoteList() {}
  RemoteList(const RemoteList &) = delete;

  RemotePtr getByName(const std::string &name) const;
  std::vector<RemotePtr> getEnabled() const;

  bool contains(const RemotePtr &) const;
  void add(const RemotePtr &);
  void remove(const RemotePtr &);

  bool empty() const { return m_remotes.empty(); }
  size_t size() const { return m_remotes.size(); }

  auto begin() { return m_remotes.begin(); }
  auto begin() const { return m_remotes.begin(); }
  auto end() { return m_remotes.end(); }
  auto end() const { return m_remotes.end(); }

private:
  std::vector<RemotePtr> m_remotes;
};

#endif
