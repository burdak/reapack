#ifndef REAPACK_TRANSACTION_HPP
#define REAPACK_TRANSACTION_HPP

#include "database.hpp"
#include "download.hpp"
#include "path.hpp"
#include "registry.hpp"
#include "remote.hpp"

#include <boost/signals2.hpp>

class PackageTransaction;

class Transaction {
public:
  typedef boost::signals2::signal<void ()> Signal;
  typedef Signal::slot_type Callback;

  typedef std::pair<Package *, const Registry::QueryResult> PackageEntry;
  typedef std::vector<PackageEntry> PackageEntryList;

  struct Error {
    std::string message;
    std::string title;
  };

  typedef std::vector<const Error> ErrorList;

  Transaction(Registry *reg, const Path &root);
  ~Transaction();

  void onReady(const Callback &callback) { m_onReady.connect(callback); }
  void onFinish(const Callback &callback) { m_onFinish.connect(callback); }
  void onDestroy(const Callback &callback) { m_onDestroy.connect(callback); }

  void fetch(const RemoteMap &);
  void fetch(const Remote &);
  void run();
  void cancel();

  bool isCancelled() const { return m_isCancelled; }
  DownloadQueue *downloadQueue() { return &m_queue; }
  const PackageEntryList &packages() const { return m_packages; }
  const PackageEntryList &newPackages() const { return m_new; }
  const PackageEntryList &updates() const { return m_updates; }
  const ErrorList &errors() const { return m_errors; }

private:
  friend PackageTransaction;

  void prepare();
  void finish();

  void saveDatabase(Download *);
  bool saveFile(Download *, const Path &);
  void addError(const std::string &msg, const std::string &title);
  Path prefixPath(const Path &) const;
  bool allFilesExists(const std::vector<Path> &) const;
  void registerFiles(const std::vector<Path> &);

  Registry *m_registry;

  Path m_root;
  Path m_dbPath;
  bool m_isCancelled;

  DatabaseList m_databases;
  DownloadQueue m_queue;
  PackageEntryList m_packages;
  PackageEntryList m_new;
  PackageEntryList m_updates;
  ErrorList m_errors;

  std::vector<PackageTransaction *> m_transactions;
  std::vector<Path> m_files;
  bool m_hasConflicts;

  Signal m_onReady;
  Signal m_onFinish;
  Signal m_onDestroy;
};

#endif
