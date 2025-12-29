// Example: Global variables and property accessors
//
// Demonstrates:
// - Read-only global constant (VERSION)
// - Read-write global variable
// - Accessor properties with getters/setters

#include <stdio.h>
#include <stdlib.h>

#include "libplatform/libplatform.h"
#include "v8.h"

// Counter that tracks access (stored in C++)
static int access_count = 0;
static double stored_value = 42.0;

// Getter for 'accessCount' - returns how many times it's been read
void AccessCountGetter(v8::Local<v8::Name> property,
                       const v8::PropertyCallbackInfo<v8::Value>& info) {
  access_count++;
  printf("[C++] accessCount getter called (count: %d)\n", access_count);
  info.GetReturnValue().Set(access_count);
}

// Getter for 'value'
void ValueGetter(v8::Local<v8::Name> property,
                 const v8::PropertyCallbackInfo<v8::Value>& info) {
  printf("[C++] value getter called, returning %.2f\n", stored_value);
  info.GetReturnValue().Set(stored_value);
}

// Setter for 'value'
void ValueSetter(v8::Local<v8::Name> property, v8::Local<v8::Value> value,
                 const v8::PropertyCallbackInfo<void>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  double new_value = value->NumberValue(context).FromMaybe(0);
  printf("[C++] value setter called, changing %.2f -> %.2f\n",
         stored_value, new_value);
  stored_value = new_value;
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

    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

    // Read-only constant: VERSION
    global->Set(isolate, "VERSION",
                v8::String::NewFromUtf8Literal(isolate, "1.0.0"),
                v8::PropertyAttribute::ReadOnly);

    // Accessor: accessCount (read-only, tracks access)
    global->SetNativeDataProperty(
        v8::String::NewFromUtf8Literal(isolate, "accessCount"),
        AccessCountGetter);

    // Accessor: value (read-write with custom getter/setter)
    global->SetNativeDataProperty(
        v8::String::NewFromUtf8Literal(isolate, "value"),
        ValueGetter,
        ValueSetter);

    v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
    v8::Context::Scope context_scope(context);

    // Simple print function for the demo
    auto print_fn = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
      v8::Isolate* isolate = info.GetIsolate();
      for (int i = 0; i < info.Length(); i++) {
        if (i > 0) printf(" ");
        v8::String::Utf8Value str(isolate, info[i]);
        printf("%s", *str ? *str : "(error)");
      }
      printf("\n");
    };

    v8::Local<v8::Function> print =
        v8::Function::New(context, print_fn).ToLocalChecked();
    context->Global()
        ->Set(context, v8::String::NewFromUtf8Literal(isolate, "print"), print)
        .Check();

    const char* code = R"(
      print('VERSION:', VERSION);

      // Try to modify VERSION (won't work - it's read-only)
      VERSION = '2.0.0';
      print('VERSION after assignment:', VERSION);

      print('');
      print('Reading accessCount multiple times:');
      print('  accessCount:', accessCount);
      print('  accessCount:', accessCount);
      print('  accessCount:', accessCount);

      print('');
      print('Using value accessor:');
      print('  Initial value:', value);
      value = 100;
      print('  After setting to 100:', value);
      value = value * 2;
      print('  After doubling:', value);
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
