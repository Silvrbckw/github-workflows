#if HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
#include "bindingdata.h"
#include <base_object-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <node.h>
#include <node_errors.h>
#include <node_external_reference.h>
#include <node_mem-inl.h>
#include <node_realm-inl.h>
#include <v8.h>

namespace node {

using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

namespace quic {

BindingData& BindingData::Get(Environment* env) {
  return *Realm::GetBindingData<BindingData>(env->context());
}

BindingData::operator ngtcp2_mem() {
  return MakeAllocator();
}

BindingData::operator nghttp3_mem() {
  ngtcp2_mem allocator = *this;
  nghttp3_mem http3_allocator = {
      allocator.user_data,
      allocator.malloc,
      allocator.free,
      allocator.calloc,
      allocator.realloc,
  };
  return http3_allocator;
}

void BindingData::CheckAllocatedSize(size_t previous_size) const {
  CHECK_GE(current_ngtcp2_memory_, previous_size);
}

void BindingData::IncreaseAllocatedSize(size_t size) {
  current_ngtcp2_memory_ += size;
}

void BindingData::DecreaseAllocatedSize(size_t size) {
  current_ngtcp2_memory_ -= size;
}

void BindingData::Initialize(Environment* env, Local<Object> target) {
  SetMethod(env->context(), target, "setCallbacks", SetCallbacks);
  Realm::GetCurrent(env->context())
      ->AddBindingData<BindingData>(env->context(), target);
}

void BindingData::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(SetCallbacks);
}

BindingData::BindingData(Realm* realm, Local<Object> object)
    : BaseObject(realm, object) {
  MakeWeak();
}

void BindingData::MemoryInfo(MemoryTracker* tracker) const {
#define V(name, _) tracker->TrackField(#name, name##_callback());

  QUIC_JS_CALLBACKS(V)

#undef V

#define V(name, _) tracker->TrackField(#name, name##_string());

  QUIC_STRINGS(V)

#undef V
}

#define V(name)                                                                \
  void BindingData::set_##name##_constructor_template(                         \
      Local<FunctionTemplate> tmpl) {                                          \
    name##_constructor_template_.Reset(env()->isolate(), tmpl);                \
  }                                                                            \
  Local<FunctionTemplate> BindingData::name##_constructor_template() const {   \
    return PersistentToLocal::Default(env()->isolate(),                        \
                                      name##_constructor_template_);           \
  }

QUIC_CONSTRUCTORS(V)

#undef V

#define V(name, _)                                                             \
  void BindingData::set_##name##_callback(Local<Function> fn) {                \
    name##_callback_.Reset(env()->isolate(), fn);                              \
  }                                                                            \
  Local<Function> BindingData::name##_callback() const {                       \
    return PersistentToLocal::Default(env()->isolate(), name##_callback_);     \
  }

QUIC_JS_CALLBACKS(V)

#undef V

#define V(name, value)                                                         \
  Local<String> BindingData::name##_string() const {                           \
    if (name##_string_.IsEmpty())                                              \
      name##_string_.Set(env()->isolate(),                                     \
                         OneByteString(env()->isolate(), value));              \
    return name##_string_.Get(env()->isolate());                               \
  }

QUIC_STRINGS(V)

#undef V

#define V(name, value)                                                         \
  Local<String> BindingData::on_##name##_string() const {                      \
    if (on_##name##_string_.IsEmpty())                                         \
      on_##name##_string_.Set(                                                 \
          env()->isolate(),                                                    \
          FIXED_ONE_BYTE_STRING(env()->isolate(), "on" #value));               \
    return on_##name##_string_.Get(env()->isolate());                          \
  }

QUIC_JS_CALLBACKS(V)

#undef V

void BindingData::SetCallbacks(const FunctionCallbackInfo<Value>& args) {
  auto env = Environment::GetCurrent(args);
  auto isolate = env->isolate();
  BindingData& state = BindingData::Get(env);
  CHECK(args[0]->IsObject());
  Local<Object> obj = args[0].As<Object>();

#define V(name, key)                                                           \
  do {                                                                         \
    Local<Value> val;                                                          \
    if (!obj->Get(env->context(), state.on_##name##_string()).ToLocal(&val) || \
        !val->IsFunction()) {                                                  \
      return THROW_ERR_MISSING_ARGS(isolate, "Missing Callback: on" #key);     \
    }                                                                          \
    state.set_##name##_callback(val.As<Function>());                           \
  } while (0);

  QUIC_JS_CALLBACKS(V)

#undef V
}

}  // namespace quic
}  // namespace node

#endif  // HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
