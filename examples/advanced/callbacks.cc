// Example: Exposing C++ functions as callbacks to JavaScript
//
// Demonstrates:
// - Simple void function (Print)
// - Function with return value (Add)
// - Function accessing arguments

#include <stdio.h>
#include <stdlib.h>

#include "libplatform/libplatform.h"
#include "v8.h"

// Callback: Print arguments to stdout
void Print(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);

  for (int i = 0; i < info.Length(); i++) {
    if (i > 0) printf(" ");
    v8::String::Utf8Value str(isolate, info[i]);
    printf("%s", *str ? *str : "(error)");
  }
  printf("\n");
}

// Callback: Add two numbers and return the result
void Add(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();

  if (info.Length() < 2) {
    isolate->ThrowError("add() requires two arguments");
    return;
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  double a = info[0]->NumberValue(context).FromMaybe(0);
  double b = info[1]->NumberValue(context).FromMaybe(0);

  info.GetReturnValue().Set(a + b);
}

// Callback: Get the length of a string
void StringLength(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();

  if (info.Length() < 1 || !info[0]->IsString()) {
    isolate->ThrowError("stringLength() requires a string argument");
    return;
  }

  v8::Local<v8::String> str = info[0].As<v8::String>();
  info.GetReturnValue().Set(str->Length());
}

int main(int argc, char* argv[]) {
  // Initialize V8
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

    // Create a template for the global object with our functions
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

    // Bind C++ functions to JavaScript names
    global->Set(isolate, "print",
                v8::FunctionTemplate::New(isolate, Print));
    global->Set(isolate, "add",
                v8::FunctionTemplate::New(isolate, Add));
    global->Set(isolate, "stringLength",
                v8::FunctionTemplate::New(isolate, StringLength));

    // Create context with our global template
    v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
    v8::Context::Scope context_scope(context);

    // Test the callbacks
    const char* code = R"(
      print('Hello from JavaScript!');
      print('Adding numbers:', add(3, 4));
      print('String length of "hello":', stringLength('hello'));
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
