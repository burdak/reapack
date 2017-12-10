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

#ifndef REAPACK_TRANSACTION_HPP
#define REAPACK_TRANSACTION_HPP

#include "receipt.hpp"
#include "registry.hpp"
#include "task.hpp"
#include "thread.hpp"

#include <boost/optional.hpp>
#include <boost/signals2.hpp>
#include <functional>
#include <memory>
#include <set>
#include <unordered_set>

class ArchiveReader;
class InstallTask;
class Path;
class Remote;
class SynchronizeTask;
class UninstallTask;

typedef std::shared_ptr<Task> TaskPtr;
typedef std::shared_ptr<Remote> RemotePtr;

struct HostTicket { bool add; Registry::Entry entry; Registry::File file; };

class Transaction {
public:
  typedef boost::signals2::signal<void ()> VoidSignal;
  typedef std::function<void()> CleanupHandler;
  typedef std::function<bool(std::vector<Registry::Entry> &)> ObsoleteHandler;

  Transaction();

  void onFinish(const VoidSignal::slot_type &slot) { m_onFinish.connect(slot); }
  void setCleanupHandler(const CleanupHandler &cb) { m_cleanupHandler = cb; }
  void setObsoleteHandler(const ObsoleteHandler &cb) { m_promptObsolete = cb; }

  void fetchIndex(const RemotePtr &, bool stale = false);
  void synchronize(const RemotePtr &,
    boost::optional<bool> forceAutoInstall = boost::none);
  void install(const Version *, bool pin = false, const ArchiveReaderPtr & = nullptr);
  void install(const Version *, const Registry::Entry &oldEntry,
    bool pin = false, const ArchiveReaderPtr & = nullptr);
  void setPinned(const Registry::Entry &, bool pinned);
  void uninstall(const RemotePtr &);
  void uninstall(const Registry::Entry &);
  void exportArchive(const std::string &path);
  bool runTasks();

  bool isCancelled() const { return m_isCancelled; }

  Receipt *receipt() { return &m_receipt; }
  Registry *registry() { return &m_registry; }
  ThreadPool *threadPool() { return &m_threadPool; }

protected:
  friend SynchronizeTask;
  friend InstallTask;
  friend UninstallTask;

  IndexPtr loadIndex(const RemotePtr &);
  void addIndex(const IndexPtr &index) { m_indexes.push_back(index); }
  void addObsolete(const Registry::Entry &e) { m_obsolete.insert(e); }
  void registerAll(bool add, const Registry::Entry &);
  void registerFile(const HostTicket &t) { m_regQueue.push(t); }

private:
  class CompareTask {
  public:
    bool operator()(const TaskPtr &l, const TaskPtr &r) const
    {
      return *l < *r;
    }
  };

  typedef std::priority_queue<TaskPtr,
    std::vector<TaskPtr>, CompareTask> TaskQueue;

  void registerQueued();
  void registerScript(const HostTicket &, bool isLast);
  void inhibit(const RemotePtr &);
  void promptObsolete();
  void runQueue(TaskQueue &queue);
  bool commitTasks();
  void finish();

  bool m_isCancelled;
  Registry m_registry;
  Receipt m_receipt;

  std::vector<IndexPtr> m_indexes; // keep alive until the transaction is destroyed
  std::unordered_set<std::string> m_inhibited;
  std::unordered_set<Registry::Entry> m_obsolete;

  ThreadPool m_threadPool;
  TaskQueue m_nextQueue;
  std::queue<TaskQueue> m_taskQueues;
  std::queue<TaskPtr> m_runningTasks;
  std::queue<HostTicket> m_regQueue;

  VoidSignal m_onFinish;
  CleanupHandler m_cleanupHandler;
  ObsoleteHandler m_promptObsolete;
};

#endif
