#include "todo_priv.h"
#include "socket_driver_priv.h" // for SocketDriverPriv

#include <algorithm> // for std::find_if

namespace sockpuppet {

namespace {
  struct WhenBefore
  {
    TimePoint when;

    bool operator()(ToDoShared const &todo) const
    {
      return (when < todo->when);
    }
  };

  struct IsSame
  {
    ToDo::ToDoPriv *ptr;

    bool operator()(ToDoShared const &todo) const
    {
      return (todo.get() == ptr);
    }
  };
} // unnamed namespace

void ToDos::Insert(ToDoShared todo)
{
  auto where = Find(WhenBefore{todo->when});
  (void)emplace(where, std::move(todo));
}

void ToDos::Remove(ToDo::ToDoPriv *todo)
{
  auto it = Find(IsSame{todo});
  if(it != end()) { // may have already been removed
    (void)erase(it);
  }
}

void ToDos::Move(ToDoShared todo, TimePoint when)
{
  Remove(todo.get());

  todo->when = when;
  Insert(std::move(todo));
}

template<typename Pred>
std::deque<ToDoShared>::iterator ToDos::Find(Pred pred)
{
  return std::find_if(begin(), end(), pred);
}


ToDo::ToDoPriv::ToDoPriv(DriverShared &driver, std::function<void()> what)
  : driver(driver)
  , what(std::move(what))
{
}

ToDo::ToDoPriv::ToDoPriv(DriverShared &driver, std::function<void()> what, TimePoint when)
  : driver(driver)
  , what(std::move(what))
  , when(when)
{
}

ToDo::ToDoPriv::~ToDoPriv() = default;

void ToDo::ToDoPriv::Cancel()
{
  if(auto const ptr = driver.lock()) {
    ptr->ToDoRemove(this);
  }
}

void ToDo::ToDoPriv::Shift(TimePoint when)
{
  if(auto const ptr = driver.lock()) {
    ptr->ToDoMove(shared_from_this(), when);
  }
}

} // namespace sockpuppet
