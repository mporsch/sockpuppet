#ifndef SOCKPUPPET_TODO_IMPL_H
#define SOCKPUPPET_TODO_IMPL_H

#include "sockpuppet/socket_async.h" // for ToDo

#include <deque> // for std::deque
#include <functional> // for std::function
#include <memory> // for std::shared_ptr

namespace sockpuppet {

using ToDoShared = std::shared_ptr<ToDo::ToDoImpl>;

// list of ToDo elements sorted by scheduled time
struct ToDos : public std::deque<ToDoShared>
{
  void Insert(ToDoShared todo);
  void Remove(ToDo::ToDoImpl *todo);
  void Move(ToDoShared todo, TimePoint when);

  template<typename Pred>
  std::deque<ToDoShared>::iterator Find(Pred);
};

struct ToDo::ToDoImpl : public std::enable_shared_from_this<ToDoImpl>
{
  using DriverShared = std::shared_ptr<Driver::DriverImpl>;

  std::weak_ptr<Driver::DriverImpl> driver;
  std::function<void()> what;
  TimePoint when;

  ToDoImpl(DriverShared &driver, std::function<void()> what);
  ToDoImpl(DriverShared &driver, std::function<void()> what, TimePoint when);
  ToDoImpl(ToDoImpl const &) = delete;
  ToDoImpl(ToDoImpl &&) = delete;
  ~ToDoImpl();
  ToDoImpl &operator=(ToDoImpl const &) = delete;
  ToDoImpl &operator=(ToDoImpl &&) = delete;

  void Cancel();

  void Shift(TimePoint when);
};

} // namespace sockpuppet

#endif // SOCKPUPPET_TODO_IMPL_H
