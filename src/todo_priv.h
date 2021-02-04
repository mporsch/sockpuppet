#ifndef SOCKPUPPET_TODO_PRIV_H
#define SOCKPUPPET_TODO_PRIV_H

#include "sockpuppet/socket_async.h" // for ToDo

#include <deque> // for std::deque
#include <functional> // for std::function
#include <memory> // for std::shared_ptr

namespace sockpuppet {

using ToDoShared = std::shared_ptr<ToDo::ToDoPriv>;

// list of ToDo elements sorted by scheduled time
struct ToDos : public std::deque<ToDoShared>
{
  void Insert(ToDoShared todo);
  void Remove(ToDo::ToDoPriv *todo);
  void Move(ToDoShared todo, TimePoint when);

  template<typename Pred>
  std::deque<ToDoShared>::iterator Find(Pred);
};

struct ToDo::ToDoPriv : public std::enable_shared_from_this<ToDoPriv>
{
  using DriverShared = std::shared_ptr<SocketDriver::SocketDriverPriv>;

  std::weak_ptr<SocketDriver::SocketDriverPriv> driver;
  std::function<void()> what;
  TimePoint when;

  ToDoPriv(DriverShared &driver, std::function<void()> what);
  ToDoPriv(DriverShared &driver, std::function<void()> what, TimePoint when);
  ToDoPriv(ToDoPriv const &) = delete;
  ToDoPriv(ToDoPriv &&) = delete;
  ~ToDoPriv();
  ToDoPriv &operator=(ToDoPriv const &) = delete;
  ToDoPriv &operator=(ToDoPriv &&) = delete;

  void Cancel();

  void Shift(TimePoint when);
};

} // namespace sockpuppet

#endif // SOCKPUPPET_TODO_PRIV_H
