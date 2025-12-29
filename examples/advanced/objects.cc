// Example: Custom objects with methods
//
// Demonstrates:
// - Creating object templates with properties
// - Adding methods to objects
// - Using FunctionTemplate for constructors

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "libplatform/libplatform.h"
#include "v8.h"

// Method: Calculate distance from origin
void PointDistance(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  // Get 'this' object and extract x, y
  v8::Local<v8::Object> self = info.This();
  double x = self->Get(context, v8::String::NewFromUtf8Literal(isolate, "x"))
                 .ToLocalChecked()
                 ->NumberValue(context)
                 .FromMaybe(0);
  double y = self->Get(context, v8::String::NewFromUtf8Literal(isolate, "y"))
                 .ToLocalChecked()
                 ->NumberValue(context)
                 .FromMaybe(0);

  double distance = sqrt(x * x + y * y);
  info.GetReturnValue().Set(distance);
}

// Method: Return string representation
void PointToString(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Object> self = info.This();
  double x = self->Get(context, v8::String::NewFromUtf8Literal(isolate, "x"))
                 .ToLocalChecked()
                 ->NumberValue(context)
                 .FromMaybe(0);
  double y = self->Get(context, v8::String::NewFromUtf8Literal(isolate, "y"))
                 .ToLocalChecked()
                 ->NumberValue(context)
                 .FromMaybe(0);

  char buffer[64];
  snprintf(buffer, sizeof(buffer), "Point(%.2f, %.2f)", x, y);
  info.GetReturnValue().Set(
      v8::String::NewFromUtf8(isolate, buffer).ToLocalChecked());
}

// Constructor: Create a new Point
void PointConstructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (!info.IsConstructCall()) {
    isolate->ThrowError("Point must be called with 'new'");
    return;
  }

  double x = 0, y = 0;
  if (info.Length() >= 1) {
    x = info[0]->NumberValue(context).FromMaybe(0);
  }
  if (info.Length() >= 2) {
    y = info[1]->NumberValue(context).FromMaybe(0);
  }

  v8::Local<v8::Object> self = info.This();
  self->Set(context, v8::String::NewFromUtf8Literal(isolate, "x"),
            v8::Number::New(isolate, x)).Check();
  self->Set(context, v8::String::NewFromUtf8Literal(isolate, "y"),
            v8::Number::New(isolate, y)).Check();
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

    // Create Point constructor template
    v8::Local<v8::FunctionTemplate> point_template =
        v8::FunctionTemplate::New(isolate, PointConstructor);
    point_template->SetClassName(
        v8::String::NewFromUtf8Literal(isolate, "Point"));

    // Get the prototype template and add methods
    v8::Local<v8::ObjectTemplate> proto = point_template->PrototypeTemplate();
    proto->Set(isolate, "distance",
               v8::FunctionTemplate::New(isolate, PointDistance));
    proto->Set(isolate, "toString",
               v8::FunctionTemplate::New(isolate, PointToString));

    // Create global object template
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);

    // Add Point constructor to global
    global->Set(isolate, "Point", point_template);

    // Add print function
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
      // Create points using the constructor
      let p1 = new Point(3, 4);
      let p2 = new Point(1, 1);

      print('p1:', p1.toString());
      print('p2:', p2.toString());

      print('p1.x:', p1.x);
      print('p1.y:', p1.y);

      print('Distance from origin:');
      print('  p1.distance():', p1.distance());
      print('  p2.distance():', p2.distance());

      // Modify point
      p1.x = 6;
      p1.y = 8;
      print('After modifying p1:', p1.toString());
      print('  p1.distance():', p1.distance());
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
