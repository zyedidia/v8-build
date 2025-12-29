// Example: Calling JavaScript functions from C++
//
// Demonstrates:
// - Getting a JS function from global scope
// - Calling JS functions with arguments
// - Handling return values
// - Calling JS callbacks passed to C++ functions

#include <stdio.h>
#include <stdlib.h>

#include "libplatform/libplatform.h"
#include "v8.h"

// Helper to get a function from global scope
v8::MaybeLocal<v8::Function> GetFunction(v8::Isolate* isolate,
                                          v8::Local<v8::Context> context,
                                          const char* name) {
  v8::Local<v8::String> func_name =
      v8::String::NewFromUtf8(isolate, name).ToLocalChecked();
  v8::Local<v8::Value> func_val;
  if (!context->Global()->Get(context, func_name).ToLocal(&func_val) ||
      !func_val->IsFunction()) {
    return {};
  }
  return func_val.As<v8::Function>();
}

// C++ function that accepts a callback and calls it
void CallWithCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (info.Length() < 1 || !info[0]->IsFunction()) {
    isolate->ThrowError("callWithCallback requires a function argument");
    return;
  }

  v8::Local<v8::Function> callback = info[0].As<v8::Function>();

  // Call the callback with some arguments
  printf("[C++] Calling JS callback with arguments (10, 20)...\n");

  v8::Local<v8::Value> argv[2] = {
      v8::Number::New(isolate, 10),
      v8::Number::New(isolate, 20)
  };

  v8::Local<v8::Value> result;
  if (callback->Call(context, context->Global(), 2, argv).ToLocal(&result)) {
    v8::String::Utf8Value str(isolate, result);
    printf("[C++] Callback returned: %s\n", *str ? *str : "(error)");
    info.GetReturnValue().Set(result);
  }
}

// C++ function that iterates an array using a JS callback
void ForEach(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (info.Length() < 2 || !info[0]->IsArray() || !info[1]->IsFunction()) {
    isolate->ThrowError("forEach requires (array, callback)");
    return;
  }

  v8::Local<v8::Array> array = info[0].As<v8::Array>();
  v8::Local<v8::Function> callback = info[1].As<v8::Function>();

  printf("[C++] Iterating array with %d elements...\n", array->Length());

  for (uint32_t i = 0; i < array->Length(); i++) {
    v8::Local<v8::Value> element;
    if (!array->Get(context, i).ToLocal(&element)) continue;

    v8::Local<v8::Value> argv[2] = {
        element,
        v8::Number::New(isolate, i)
    };

    callback->Call(context, context->Global(), 2, argv).ToLocalChecked();
  }
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
    global->Set(isolate, "callWithCallback",
                v8::FunctionTemplate::New(isolate, CallWithCallback));
    global->Set(isolate, "forEach",
                v8::FunctionTemplate::New(isolate, ForEach));

    v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
    v8::Context::Scope context_scope(context);

    // First, define some JS functions
    const char* setup_code = R"(
      function add(a, b) {
        return a + b;
      }

      function multiply(a, b) {
        return a * b;
      }

      function greet(name) {
        return 'Hello, ' + name + '!';
      }
    )";

    v8::Local<v8::String> setup_source =
        v8::String::NewFromUtf8(isolate, setup_code).ToLocalChecked();
    v8::Script::Compile(context, setup_source)
        .ToLocalChecked()
        ->Run(context)
        .ToLocalChecked();

    // Now call JS functions from C++
    printf("=== Calling JS functions from C++ ===\n\n");

    // Call 'add' function
    {
      v8::Local<v8::Function> add_fn;
      if (GetFunction(isolate, context, "add").ToLocal(&add_fn)) {
        v8::Local<v8::Value> argv[2] = {
            v8::Number::New(isolate, 5),
            v8::Number::New(isolate, 3)
        };
        v8::Local<v8::Value> result =
            add_fn->Call(context, context->Global(), 2, argv).ToLocalChecked();
        printf("add(5, 3) = %g\n", result->NumberValue(context).FromMaybe(0));
      }
    }

    // Call 'multiply' function
    {
      v8::Local<v8::Function> mul_fn;
      if (GetFunction(isolate, context, "multiply").ToLocal(&mul_fn)) {
        v8::Local<v8::Value> argv[2] = {
            v8::Number::New(isolate, 7),
            v8::Number::New(isolate, 6)
        };
        v8::Local<v8::Value> result =
            mul_fn->Call(context, context->Global(), 2, argv).ToLocalChecked();
        printf("multiply(7, 6) = %g\n",
               result->NumberValue(context).FromMaybe(0));
      }
    }

    // Call 'greet' function
    {
      v8::Local<v8::Function> greet_fn;
      if (GetFunction(isolate, context, "greet").ToLocal(&greet_fn)) {
        v8::Local<v8::Value> argv[1] = {
            v8::String::NewFromUtf8Literal(isolate, "World")
        };
        v8::Local<v8::Value> result =
            greet_fn->Call(context, context->Global(), 1, argv).ToLocalChecked();
        v8::String::Utf8Value str(isolate, result);
        printf("greet('World') = '%s'\n", *str);
      }
    }

    printf("\n=== JS code using C++ callback functions ===\n\n");

    // Test the callback functions from JS side
    const char* test_code = R"(
      // Pass a callback to C++
      let result = callWithCallback(function(a, b) {
        return a + b;
      });

      // Use forEach with array and callback
      forEach([1, 2, 3, 4, 5], function(value, index) {
        // This callback is called from C++ for each element
      });
    )";

    v8::Local<v8::String> test_source =
        v8::String::NewFromUtf8(isolate, test_code).ToLocalChecked();
    v8::Script::Compile(context, test_source)
        .ToLocalChecked()
        ->Run(context)
        .ToLocalChecked();
  }

  isolate->Dispose();
  delete create_params.array_buffer_allocator;
  v8::V8::Dispose();
  v8::V8::DisposePlatform();

  return 0;
}
