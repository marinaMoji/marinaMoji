#ifndef MOZC_SYNC_SYNC_SCHEDULER_H_
#define MOZC_SYNC_SYNC_SCHEDULER_H_

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

namespace mozc {
namespace sync {

// Background sync scheduler running in mozc_server.
class SyncScheduler {
 public:
  using SyncCallback = std::function<bool()>;

  explicit SyncScheduler(SyncCallback callback);
  ~SyncScheduler();

  SyncScheduler(const SyncScheduler&) = delete;
  SyncScheduler& operator=(const SyncScheduler&) = delete;

  void Start();
  void Stop();
  void NotifyShutdown();

 private:
  SyncCallback callback_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> shutdown_sync_pending_{false};
  std::unique_ptr<std::thread> thread_;
};

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_SCHEDULER_H_
