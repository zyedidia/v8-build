// Example: Exception handling with TryCatch
//
// Demonstrates:
// - Catching JavaScript exceptions in C++
// - Throwing errors from C++ to JavaScript
// - Getting exception details (message, stack trace, line number)

#include <stdio.h>
#include <stdlib.h>

#include "libplatform/libplatform.h"
#include "v8.h"

// Helper to extract C string from V8 string
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<failed to convert>";
}

// Report exception details
void ReportException(v8::Isolate* isolate, v8::TryCatch* try_catch) {
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::String::Utf8Value exception(isolate, try_catch->Exception());
  printf("Exception: %s\n", ToCString(exception));

  v8::Local<v8::Message> message = try_catch->Message();
  if (!message.IsEmpty()) {
    // Print location
    v8::String::Utf8Value filename(isolate,
        message->GetScriptOrigin().ResourceName());
    int linenum = message->GetLineNumber(context).FromMaybe(-1);
    printf("  at %s:%d\n", ToCString(filename), linenum);

    // Print source line
    v8::Local<v8::String> sourceline;
    if (message->GetSourceLine(context).ToLocal(&sourceline)) {
      v8::String::Utf8Value line(isolate, sourceline);
      printf("  > %s\n", ToCString(line));
    }

    // Print stack trace if available
    v8::Local<v8::Value> stack_trace;
    if (try_catch->StackTrace(context).ToLocal(&stack_trace) &&
        stack_trace->IsString()) {
      v8::String::Utf8Value stack(isolate, stack_trace);
      printf("\nStack trace:\n%s\n", ToCString(stack));
    }
  }
}

// Execute script and handle exceptions
bool ExecuteScript(v8::Isolate* isolate, v8::Local<v8::Context> context,
                   const char* code, const char* name) {
  v8::HandleScope handle_scope(isolate);
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(isolate, code).ToLocalChecked();
  v8::Local<v8::String> script_name =
      v8::String::NewFromUtf8(isolate, name).ToLocalChecked();
  v8::ScriptOrigin origin(script_name);

  v8::Local<v8::Script> script;
  if (!v8::Script::Compile(context, source, &origin).ToLocal(&script)) {
    printf("=== Compilation Error ===\n");
    ReportException(isolate, &try_catch);
    return false;
  }

  v8::Local<v8::Value> result;
  if (!script->Run(context).ToLocal(&result)) {
    printf("=== Runtime Error ===\n");
    ReportException(isolate, &try_catch);
    return false;
  }

  return true;
}

// C++ function that throws an error
void ThrowError(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  isolate->ThrowError("Error thrown from C++!");
}

// C++ function that throws a TypeError
void ThrowTypeError(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  isolate->ThrowException(v8::Exception::TypeError(
      v8::String::NewFromUtf8Literal(isolate, "Type error from C++")));
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
    global->Set(isolate, "throwError",
                v8::FunctionTemplate::New(isolate, ThrowError));
    global->Set(isolate, "throwTypeError",
                v8::FunctionTemplate::New(isolate, ThrowTypeError));

    v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
    v8::Context::Scope context_scope(context);

    printf("--- Test 1: Syntax Error ---\n");
    ExecuteScript(isolate, context,
                  "let x = ;",  // Invalid syntax
                  "syntax_error.js");

    printf("\n--- Test 2: Reference Error ---\n");
    ExecuteScript(isolate, context,
                  "console.log(undefinedVariable);",
                  "reference_error.js");

    printf("\n--- Test 3: Error thrown from C++ ---\n");
    ExecuteScript(isolate, context,
                  "throwError();",
                  "cpp_error.js");

    printf("\n--- Test 4: TypeError from C++ ---\n");
    ExecuteScript(isolate, context,
                  "throwTypeError();",
                  "cpp_type_error.js");

    printf("\n--- Test 5: Error in nested function call ---\n");
    ExecuteScript(isolate, context, R"(
      function foo() {
        bar();
      }
      function bar() {
        throw new Error('Nested error');
      }
      foo();
    )", "nested_error.js");

    printf("\n--- Test 6: Successful execution ---\n");
    if (ExecuteScript(isolate, context, "1 + 1", "success.js")) {
      printf("Script executed successfully!\n");
    }
  }

  isolate->Dispose();
  delete create_params.array_buffer_allocator;
  v8::V8::Dispose();
  v8::V8::DisposePlatform();

  return 0;
}
