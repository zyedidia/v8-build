// Example: Wrapping C++ classes
//
// Demonstrates:
// - Using internal fields to store C++ object pointers
// - Constructor that creates and wraps C++ objects
// - Instance methods that operate on wrapped data
// - Weak callbacks for destructor cleanup

#include <stdio.h>
#include <stdlib.h>

#include "libplatform/libplatform.h"
#include "v8.h"

// A simple C++ class to wrap
class Counter {
 public:
  Counter(int initial) : value_(initial) {
    printf("[C++] Counter created with initial value %d\n", value_);
  }

  ~Counter() {
    printf("[C++] Counter destroyed (final value was %d)\n", value_);
  }

  void Increment() { value_++; }
  void Decrement() { value_--; }
  void Add(int n) { value_ += n; }
  int Value() const { return value_; }

 private:
  int value_;
};

// Helper to unwrap C++ object from JS object
Counter* UnwrapCounter(v8::Local<v8::Object> obj) {
  v8::Local<v8::External> wrap = obj->GetInternalField(0).As<v8::External>();
  return static_cast<Counter*>(wrap->Value());
}

// Constructor
void CounterNew(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (!info.IsConstructCall()) {
    isolate->ThrowError("Counter must be called with 'new'");
    return;
  }

  int initial = 0;
  if (info.Length() >= 1) {
    initial = info[0]->Int32Value(context).FromMaybe(0);
  }

  // Create C++ object
  Counter* counter = new Counter(initial);

  // Wrap it in the JS object
  info.This()->SetInternalField(0, v8::External::New(isolate, counter));
  info.GetReturnValue().Set(info.This());
}

// Instance method: increment()
void CounterIncrement(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Counter* counter = UnwrapCounter(info.This());
  counter->Increment();
}

// Instance method: decrement()
void CounterDecrement(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Counter* counter = UnwrapCounter(info.This());
  counter->Decrement();
}

// Instance method: add(n)
void CounterAdd(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (info.Length() < 1) {
    isolate->ThrowError("add() requires one argument");
    return;
  }

  Counter* counter = UnwrapCounter(info.This());
  int n = info[0]->Int32Value(context).FromMaybe(0);
  counter->Add(n);
}

// Instance method: value() - getter
void CounterValue(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Counter* counter = UnwrapCounter(info.This());
  info.GetReturnValue().Set(counter->Value());
}

// Instance method: destroy() - explicitly clean up
void CounterDestroy(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> self = info.This();
  v8::Local<v8::External> wrap = self->GetInternalField(0).As<v8::External>();
  Counter* counter = static_cast<Counter*>(wrap->Value());
  delete counter;
  self->SetInternalField(0, v8::External::New(info.GetIsolate(), nullptr));
}

int main(int argc, char* argv[]) {
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    // Create Counter constructor template
    v8::Local<v8::FunctionTemplate> counter_template =
        v8::FunctionTemplate::New(isolate, CounterNew);
    counter_template->SetClassName(
        v8::String::NewFromUtf8Literal(isolate, "Counter"));

    // Set up internal field for storing C++ pointer
    counter_template->InstanceTemplate()->SetInternalFieldCount(1);

    // Add methods to prototype
    v8::Local<v8::ObjectTemplate> proto = counter_template->PrototypeTemplate();
    proto->Set(isolate, "increment",
               v8::FunctionTemplate::New(isolate, CounterIncrement));
    proto->Set(isolate, "decrement",
               v8::FunctionTemplate::New(isolate, CounterDecrement));
    proto->Set(isolate, "add",
               v8::FunctionTemplate::New(isolate, CounterAdd));
    proto->Set(isolate, "value",
               v8::FunctionTemplate::New(isolate, CounterValue));
    proto->Set(isolate, "destroy",
               v8::FunctionTemplate::New(isolate, CounterDestroy));

    // Create global template
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
    global->Set(isolate, "Counter", counter_template);
    global->Set(isolate, "print",
                v8::FunctionTemplate::New(isolate,
                    [](const v8::FunctionCallbackInfo<v8::Value>& info) {
                      v8::Isolate* isolate = info.GetIsolate();
                      for (int i = 0; i < info.Length(); i++) {
                        if (i > 0) printf(" ");
                        v8::String::Utf8Value str(isolate, info[i]);
                        printf("%s", *str ? *str : "(error)");
                      }
                      printf("\n");
                    }));

    v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
    v8::Context::Scope context_scope(context);

    const char* code = R"(
      print('Creating counter with initial value 10...');
      let counter = new Counter(10);

      print('Initial value:', counter.value());

      print('Calling increment() 3 times...');
      counter.increment();
      counter.increment();
      counter.increment();
      print('Value after incrementing:', counter.value());

      print('Calling add(5)...');
      counter.add(5);
      print('Value after adding 5:', counter.value());

      print('Calling decrement()...');
      counter.decrement();
      print('Final value:', counter.value());

      print('Destroying counter explicitly...');
      counter.destroy();
    )";

    v8::Local<v8::String> source =
        v8::String::NewFromUtf8(isolate, code).ToLocalChecked();
    v8::Local<v8::Script> script =
        v8::Script::Compile(context, source).ToLocalChecked();
    script->Run(context).ToLocalChecked();
  }

  isolate->Dispose();
  delete create_params.array_buffer_allocator;
  v8::V8::Dispose();
  v8::V8::DisposePlatform();

  return 0;
}
