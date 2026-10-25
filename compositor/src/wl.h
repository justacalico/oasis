#pragma once
#include <unistd.h>
#include <wayland-server-core.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace oasis {

// ------------------------------------------------------------------ helpers

template <typename T>
T* user_data(wl_resource* res) {
  return static_cast<T*>(wl_resource_get_user_data(res));
}

// wl_resource destructor that deletes the C++ object stored as user data.
template <typename T>
void delete_object(wl_resource* res) {
  delete user_data<T>(res);
}

// Request handler for plain "destroy" / "release" requests.
void destroy_request(wl_client*, wl_resource* res);

// Base for objects whose lifetime is owned by a wl_resource.
struct ResourceBound {
  wl_resource* res = nullptr;
};

// Attaches a request table to an already-created resource and ties obj's
// lifetime to it: obj is deleted when the resource is destroyed.
template <typename T>
void attach_resource(wl_resource* res, T* obj, const void* impl,
                     wl_resource_destroy_func_t dtor = &delete_object<T>) {
  obj->res = res;
  wl_resource_set_implementation(res, impl, obj, dtor);
}

// Creates a wl_resource, points it at obj, installs the request table impl,
// and arranges for obj to be deleted when the resource is destroyed.
template <typename T>
T* bind_resource(wl_client* client, const wl_interface* iface, int version,
                 uint32_t id, T* obj, const void* impl,
                 wl_resource_destroy_func_t dtor = &delete_object<T>) {
  attach_resource(wl_resource_create(client, iface, version, id), obj, impl,
                  dtor);
  return obj;
}

namespace detail {

// Turns a member function into a thunk with the exact C signature that a
// generated *_interface request table expects. Two member shapes are
// supported:
//   void (T::*)(wl_resource*, Args...)
//   void (T::*)(wl_client*, wl_resource*, Args...)
// where Args are the wire-decoded arguments.
template <typename Sig, Sig M>
struct RequestThunk;

template <typename T, typename... A, void (T::*M)(wl_resource*, A...)>
struct RequestThunk<void (T::*)(wl_resource*, A...), M> {
  static void call(wl_client*, wl_resource* r, A... a) {
    (user_data<T>(r)->*M)(r, a...);
  }
};

template <typename T, typename... A,
          void (T::*M)(wl_client*, wl_resource*, A...)>
struct RequestThunk<void (T::*)(wl_client*, wl_resource*, A...), M> {
  static void call(wl_client* c, wl_resource* r, A... a) {
    (user_data<T>(r)->*M)(c, r, a...);
  }
};

// The same, for wayland-client listener tables where the first argument is
// the void* data pointer registered with add_listener.
template <typename Sig, Sig M>
struct ListenerThunk;

template <typename T, typename... A, void (T::*M)(A...)>
struct ListenerThunk<void (T::*)(A...), M> {
  static void call(void* data, A... a) {
    (static_cast<T*>(data)->*M)(a...);
  }
};

}  // namespace detail

// Expands to a request-table entry that forwards the request to T::method on
// the resource's user-data object.
#define REQ(T, m) \
  (&::oasis::detail::RequestThunk<decltype(&T::m), &T::m>::call)

// Expands to a listener-table entry that forwards the event to T::method on
// the listener's data object.
#define LREQ(T, m) \
  (&::oasis::detail::ListenerThunk<decltype(&T::m), &T::m>::call)

// ------------------------------------------------------------------- Global

// RAII wrapper for wl_global. The bind callback receives (client, version,
// id) and is expected to create the resource.
class Global {
 public:
  using Bind = std::function<void(wl_client*, uint32_t version, uint32_t id)>;

  Global() = default;
  Global(wl_display* display, const wl_interface* iface, int version,
         Bind bind) {
    create(display, iface, version, std::move(bind));
  }
  ~Global() {
    if (global_) wl_global_destroy(global_);
  }
  Global(const Global&) = delete;
  Global& operator=(const Global&) = delete;

  bool create(wl_display* display, const wl_interface* iface, int version,
              Bind bind) {
    bind_ = std::move(bind);
    global_ = wl_global_create(display, iface, version, this, &dispatch);
    return global_ != nullptr;
  }

  explicit operator bool() const { return global_ != nullptr; }

 private:
  static void dispatch(wl_client* client, void* data, uint32_t version,
                       uint32_t id) {
    static_cast<Global*>(data)->bind_(client, version, id);
  }
  wl_global* global_ = nullptr;
  Bind bind_;
};

// -------------------------------------------------------------- EventSource

// RAII wrapper for wl_event_source. Fd watches persist until destroyed;
// timers and idle sources fire once and are freed by the event loop.
class EventSource {
 public:
  using FdFn = std::function<int(int fd, uint32_t mask)>;
  using Fn = std::function<void()>;

  ~EventSource() {
    if (src_) wl_event_source_remove(src_);
  }
  EventSource(const EventSource&) = delete;
  EventSource& operator=(const EventSource&) = delete;

  explicit operator bool() const { return src_ != nullptr; }
  void remove() {
    if (src_) {
      wl_event_source_remove(src_);
      src_ = nullptr;
    }
  }
  void update_timer(int delay_ms) {
    wl_event_source_timer_update(src_, delay_ms);
  }

  // Persistent fd watch; the returned unique_ptr owns it.
  static std::unique_ptr<EventSource> watch(wl_event_loop* loop, int fd,
                                          uint32_t mask, FdFn fn) {
    auto* s = new EventSource;
    s->fd_fn_ = std::move(fn);
    s->src_ = wl_event_loop_add_fd(loop, fd, mask, &fd_dispatch, s);
    return std::unique_ptr<EventSource>(s);
  }

  // One-shot timer; the caller keeps or drops the returned unique_ptr.
  static std::unique_ptr<EventSource> after(wl_event_loop* loop, int delay_ms,
                                            Fn fn) {
    auto* s = new EventSource;
    s->fn_ = std::move(fn);
    s->src_ = wl_event_loop_add_timer(loop, &oneshot_dispatch, s);
    s->update_timer(delay_ms);
    return std::unique_ptr<EventSource>(s);
  }

  // One-shot idle callback; same lifetime rule as after().
  static std::unique_ptr<EventSource> when_idle(wl_event_loop* loop, Fn fn) {
    auto* s = new EventSource;
    s->fn_ = std::move(fn);
    s->src_ = wl_event_loop_add_idle(loop, &idle_dispatch, s);
    return std::unique_ptr<EventSource>(s);
  }

  // Fire-and-forget timer that deletes itself after the callback runs.
  static void defer(wl_event_loop* loop, int delay_ms, Fn fn) {
    auto* s = new EventSource;
    s->self_owned_ = true;
    s->fn_ = std::move(fn);
    s->src_ = wl_event_loop_add_timer(loop, &oneshot_dispatch, s);
    s->update_timer(delay_ms);
  }

 private:
  EventSource() = default;

  static int fd_dispatch(int fd, uint32_t mask, void* data) {
    return static_cast<EventSource*>(data)->fd_fn_(fd, mask);
  }
  static int oneshot_dispatch(void* data) {
    auto* s = static_cast<EventSource*>(data);
    s->src_ = nullptr;  // one-shot sources are freed by the loop
    Fn fn = std::move(s->fn_);
    if (s->self_owned_) delete s;
    fn();
    return 0;
  }
  static void idle_dispatch(void* data) { oneshot_dispatch(data); }

  wl_event_source* src_ = nullptr;
  FdFn fd_fn_;
  Fn fn_;
  bool self_owned_ = false;
};

// ----------------------------------------------------------------- Listener

// RAII wl_listener bound to a std::function. attach() hooks it into a
// wl_signal; native() exposes it for APIs like
// wl_display_add_client_created_listener that take a wl_listener directly.
class Listener {
 public:
  using Fn = std::function<void(void* data)>;

  explicit Listener(Fn fn = {}) : fn_(std::move(fn)) {
    listener_.notify = &dispatch;
    listener_.link.prev = &listener_.link;
    listener_.link.next = &listener_.link;
  }
  ~Listener() { detach(); }
  Listener(const Listener&) = delete;
  Listener& operator=(const Listener&) = delete;

  wl_listener* native() { return &listener_; }
  void set(Fn fn) { fn_ = std::move(fn); }
  void attach(wl_signal* signal) {
    detach();
    wl_signal_add(signal, &listener_);
  }
  void detach() {
    if (!wl_list_empty(&listener_.link)) wl_list_remove(&listener_.link);
  }

 private:
  static void dispatch(wl_listener* l, void* data) {
    Listener* self;
    self = wl_container_of(l, self, listener_);
    if (self->fn_) self->fn_(data);
  }

  wl_listener listener_;
  Fn fn_;
};

// ------------------------------------------------------------------- WlArray

// Scoped wl_array that releases itself.
class WlArray {
 public:
  WlArray() { wl_array_init(&array_); }
  ~WlArray() { wl_array_release(&array_); }
  WlArray(const WlArray&) = delete;
  WlArray& operator=(const WlArray&) = delete;

  wl_array* get() { return &array_; }
  void add(uint32_t v) {
    *static_cast<uint32_t*>(wl_array_add(&array_, sizeof(v))) = v;
  }
  void add(const std::vector<uint32_t>& v) {
    for (uint32_t x : v) add(x);
  }

 private:
  wl_array array_;
};

// ------------------------------------------------------------------------ Fd

// RAII file descriptor.
class Fd {
 public:
  Fd() = default;
  explicit Fd(int fd) : fd_(fd) {}
  ~Fd() { reset(); }
  Fd(const Fd&) = delete;
  Fd& operator=(const Fd&) = delete;
  Fd(Fd&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; }
  Fd& operator=(Fd&& o) noexcept {
    reset(o.fd_);
    o.fd_ = -1;
    return *this;
  }

  int get() const { return fd_; }
  int release() { return std::exchange(fd_, -1); }
  void reset(int fd = -1) {
    if (fd_ >= 0) close(fd_);
    fd_ = fd;
  }
  explicit operator bool() const { return fd_ >= 0; }

 private:
  int fd_ = -1;
};

}  // namespace oasis
