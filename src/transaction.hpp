#ifndef REAPACK_TRANSACTION_HPP
#define REAPACK_TRANSACTION_HPP

#include "database.hpp"
#include "download.hpp"
#include "path.hpp"
#include "registry.hpp"
#include "remote.hpp"

#include <boost/signals2.hpp>

typedef std::vector<std::string> ErrorList;

class Transaction {
public:
  typedef boost::signals2::signal<void ()> Signal;
  typedef Signal::slot_type Callback;

  typedef std::pair<Package *, const Registry::QueryResult> PackageEntry;
  typedef std::vector<PackageEntry> PackageEntryList;

  Transaction(Registry *reg, const Path &root);
  ~Transaction();

  void onReady(const Callback &callback) { m_onReady.connect(callback); }
  void onFinish(const Callback &callback) { m_onFinish.connect(callback); }

  void fetch(const RemoteMap &);
  void fetch(const Remote &);

  void run();
  void cancel();

  DownloadQueue *downloadQueue() { return &m_queue; }

  const PackageEntryList &packages() const { return m_packages; }
  const PackageEntryList &newPackages() const { return m_new; }
  const PackageEntryList &updates() const { return m_updates; }
  const ErrorList &errors() const { return m_errors; }

private:
  void prepare();
  void finish();

  void install(const PackageEntry &);
  void addError(const std::string &msg, const std::string &title);
  Path installPath(Package *) const;

  Registry *m_registry;

  Path m_root;
  Path m_dbPath;

  DatabaseList m_databases;
  DownloadQueue m_queue;
  PackageEntryList m_packages;
  PackageEntryList m_new;
  PackageEntryList m_updates;
  ErrorList m_errors;

  Signal m_onReady;
  Signal m_onFinish;
};

#endif
