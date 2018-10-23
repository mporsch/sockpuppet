#ifndef RESOURCE_POOL_H
#define RESOURCE_POOL_H

#include <algorithm> // for std::find_if
#include <deque> // for std::deque
#include <memory> // for std::unique_ptr
#include <mutex> // for std::mutex
#include <queue> // for std::queue

template<typename Resource>
struct ResourceRecycler;

/// Internally stores two resource lists; busy and idle.
/// Idle resources may be obtained by the user and
/// are moved to the busy list. Once the user releases
/// a resource, it is automatically moved back again.
template<typename Resource>
class ResourcePool
{
  friend struct ResourceRecycler<Resource>;

public:
  /// Create a resource pool with given maximum number of resources.
  ResourcePool(size_t maxSize = 0U)
    : m_maxSize(maxSize - 1U)
  {
  }

  ResourcePool(ResourcePool const &other) = delete;
  ResourcePool(ResourcePool &&other) = delete;

  ResourcePool &operator=(ResourcePool const &other) = delete;
  ResourcePool &operator=(ResourcePool &&other) = delete;

  using ResourcePtr = std::unique_ptr<Resource, ResourceRecycler<Resource>>;

  /// Obtain an idle resouce or create a new one with given arguments.
  /// @return  Pointer to resource. The resource is still owned by
  ///          the resource pool; the user must not change the pointer.
  /// @throws  If more resources are obtained than initially agreed upon.
  /// @note  Mind that all resources are invalidated when destroying
  ///        the resouce pool.
  template<typename... Args>
  ResourcePtr Get(Args&&... args)
  {
    std::lock_guard<std::mutex> lock(m_mtx);

    if(m_idle.empty()) {
      if(m_busy.size() <= m_maxSize) {
        m_busy.emplace_front(
          std::make_unique<Resource>(
            std::forward<Args>(args)...));
        return {m_busy.front().get(), ResourceRecycler<Resource>{*this}};
      } else {
        throw std::runtime_error("out of resources");
      }
    } else {
      // move from idle to busy
      m_busy.emplace_front(std::move(m_idle.front()));
      m_idle.pop();
      return {m_busy.front().get(), ResourceRecycler<Resource>{*this}};
    }
  }

private:
  void Return(Resource *resource)
  {
    std::lock_guard<std::mutex> lock(m_mtx);

    auto const it = std::find_if(std::begin(m_busy), std::end(m_busy),
      [&](typename decltype(m_busy)::const_reference res) -> bool
      {
        return (res.get() == resource);
      });
    if(it == std::end(m_busy)) {
      throw std::runtime_error("returned invalid resource");
    }

    // move from busy to idle
    m_idle.push(std::move(*it));
    m_busy.erase(it);
  }

private:
  using ResourceStorage = std::unique_ptr<Resource>;
  size_t m_maxSize;
  std::queue<ResourceStorage> m_idle;
  std::deque<ResourceStorage> m_busy;
  std::mutex m_mtx;
};

template<typename Resource>
struct ResourceRecycler
{
  ResourcePool<Resource> &pool;

  void operator()(Resource *ptr)
  {
    pool.Return(ptr);
  }
};

#endif // RESOURCE_POOL_H
