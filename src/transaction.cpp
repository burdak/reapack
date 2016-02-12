/* ReaPack: Package manager for REAPER
 * Copyright (C) 2015-2016  Christian Fillion
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

#include "transaction.hpp"

#include "encoding.hpp"
#include "errors.hpp"
#include "index.hpp"
#include "remote.hpp"
#include "task.hpp"

#include <boost/algorithm/string.hpp>
#include <fstream>

#include <reaper_plugin_functions.h>

using namespace std;

Transaction::Transaction()
  : m_isCancelled(false), m_enableReport(false)
{
  m_registry = new Registry(Path::prefixCache("registry.db"));

  m_downloadQueue.onDone([=](void *) {
    if(m_installQueue.empty())
      finish();
    else
      installQueued();
  });
}

Transaction::~Transaction()
{
  for(Task *task : m_tasks)
    delete task;

  for(const RemoteIndex *ri : m_remoteIndexes)
    delete ri;

  delete m_registry;
}

void Transaction::synchronize(const Remote &remote, const bool isUserAction)
{
  // show the report dialog even if no task are ran
  if(isUserAction)
    m_enableReport = true;

  fetchIndex(remote, [=] {
    const RemoteIndex *ri;

    try {
      ri = RemoteIndex::load(remote.name());
      m_remoteIndexes.push_back(ri);
    }
    catch(const reapack_error &e) {
      // index file is invalid (load error)
      addError(e.what(), remote.name());
      return;
    }

    for(const Package *pkg : ri->packages())
      upgrade(pkg);
  });
}

void Transaction::fetchIndex(const Remote &remote, const IndexCallback &cb)
{
  m_remotes.insert({remote, cb});

  if(m_remotes.count(remote) > 1)
    return;

  Download *dl = new Download(remote.name(), remote.url());
  m_downloadQueue.push(dl);

  dl->onFinish(bind(&Transaction::saveIndex, this, dl, remote));
}

void Transaction::saveIndex(Download *dl, const Remote &remote)
{
  if(!saveFile(dl, RemoteIndex::pathFor(remote.name())))
    return;

  const auto end = m_remotes.upper_bound(remote);

  for(auto it = m_remotes.lower_bound(remote); it != end; it++)
    it->second();
}

void Transaction::upgrade(const Package *pkg)
{
  const Version *ver = pkg->lastVersion();
  auto queryRes = m_registry->query(pkg);

  if(queryRes.status == Registry::UpToDate) {
    if(allFilesExists(ver->files()))
      return; // latest version is really installed, nothing to do here!
    else
      queryRes.status = Registry::Uninstalled;
  }

  m_installQueue.push({ver, queryRes});
}

void Transaction::installQueued()
{
  while(!m_installQueue.empty()) {
    installTicket(m_installQueue.front());
    m_installQueue.pop();
  }

  runTasks();
}

void Transaction::installTicket(const InstallTicket &ticket)
{
  const Version *ver = ticket.first;
  const Registry::QueryResult &queryRes = ticket.second;
  const set<Path> &currentFiles = m_registry->getFiles(queryRes.entry);

  Registry::Entry newEntry;

  try {
    // register the new version and prevent file conflicts
    vector<Path> conflicts;
    newEntry = m_registry->push(ver, &conflicts);

    if(!conflicts.empty()) {
      for(const Path &path : conflicts) {
        addError("Conflict: " + path.join() +
          " is already owned by another package",
          ver->fullName());
      }

      return;
    }
  }
  catch(const reapack_error &e) {
    // handle database error from Registry::push
    addError(e.what(), ver->fullName());
    return;
  }

  InstallTask *task = new InstallTask(ver, currentFiles, this);

  task->onCommit([=] {
    if(queryRes.status == Registry::UpdateAvailable)
      m_updates.push_back(ticket);
    else
      m_new.push_back(ticket);

    const set<Path> &removedFiles = task->removedFiles();
    m_removals.insert(removedFiles.begin(), removedFiles.end());

    registerInHost(true, newEntry);
  });

  addTask(task);
}

void Transaction::registerAll(const Remote &remote)
{
  const vector<Registry::Entry> &entries = m_registry->getEntries(remote);

  for(const auto &entry : entries)
    registerInHost(true, entry);
}

void Transaction::unregisterAll(const Remote &remote)
{
  const vector<Registry::Entry> &entries = m_registry->getEntries(remote);

  for(const auto &entry : entries)
    registerInHost(false, entry);
}

void Transaction::uninstall(const Remote &remote)
{
  const vector<Registry::Entry> &entries = m_registry->getEntries(remote);

  if(entries.empty())
    return;

  vector<Path> allFiles;

  for(const auto &entry : entries) {
    const set<Path> &files = m_registry->getFiles(entry);
    allFiles.insert(allFiles.end(), files.begin(), files.end());

    registerInHost(false, entry);

    // forget the package even if some files cannot be removed
    m_registry->forget(entry);
  }

  RemoveTask *task = new RemoveTask(allFiles, this);

  task->onCommit([=] {
    const set<Path> &removedFiles = task->removedFiles();
    m_removals.insert(removedFiles.begin(), removedFiles.end());
  });

  addTask(task);
}

void Transaction::cancel()
{
  m_isCancelled = true;
  m_enableReport = false;

  for(Task *task : m_tasks)
    task->rollback();

  if(m_downloadQueue.idle())
    finish();
  else
    m_downloadQueue.abort();
}

bool Transaction::saveFile(Download *dl, const Path &path)
{
  if(dl->state() != Download::Success) {
    addError(dl->contents(), dl->url());
    return false;
  }

  RecursiveCreateDirectory(path.dirname().c_str(), 0);

  const string strPath = path.join();
  ofstream file(make_autostring(strPath), ios_base::binary);

  if(!file) {
    addError(strerror(errno), strPath);
    return false;
  }

  file << dl->contents();
  file.close();

  return true;
}

void Transaction::finish()
{
  // called when the download queue is done, or if there is nothing to do

  if(!m_isCancelled) {
    for(Task *task : m_tasks)
      task->commit();

    m_registry->commit();

    registerScriptsInHost();
  }

  m_onFinish();
  m_onDestroy();
}

void Transaction::addError(const string &message, const string &title)
{
  m_errors.push_back({message, title});
  m_enableReport = true;
}

bool Transaction::allFilesExists(const set<Path> &list) const
{
  for(const Path &path : list) {
    if(!file_exists(Path::prefixRoot(path).join().c_str()))
      return false;
  }

  return true;
}

void Transaction::addTask(Task *task)
{
  m_tasks.push_back(task);
  m_taskQueue.push(task);

  m_enableReport = true;
}

void Transaction::runTasks()
{
  while(!m_taskQueue.empty()) {
    m_taskQueue.front()->start();
    m_taskQueue.pop();
  }

  if(m_downloadQueue.idle())
    finish();
}

void Transaction::registerInHost(const bool add, const Registry::Entry &entry)
{
  // don't actually do anything until commit()

  switch(entry.type) {
  case Package::ScriptType:
    m_scriptRegs.push({add, entry, m_registry->getMainFile(entry)});
    break;
  default:
    break;
  }
}

void Transaction::registerScriptsInHost()
{
  if(!AddRemoveReaScript) {
    // do nothing if REAPER < v5.12
    m_scriptRegs = {};
    return;
  }

  enum Section { MainSection = 0, MidiEditorSection = 32060 };

  while(!m_scriptRegs.empty()) {
    const HostRegistration &reg = m_scriptRegs.front();
    const Registry::Entry &entry = reg.entry;
    const std::string &path = Path::prefixRoot(reg.file).join();
    const bool isLast = m_scriptRegs.size() == 1;
    Section section = MainSection;

    string category = Path(entry.category).first();
    boost::algorithm::to_lower(category);

    if(category == "midi editor")
      section = MidiEditorSection;

    if(!AddRemoveReaScript(reg.add, section, path.c_str(), isLast) && reg.add)
      addError("Script could not be registered in REAPER.", reg.file);

    m_scriptRegs.pop();
  }
}
