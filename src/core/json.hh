#ifndef SOCKET_RUNTIME_CORE_JSON_H
#define SOCKET_RUNTIME_CORE_JSON_H

#include "../platform/platform.hh"

namespace SSC::JSON {
  // forward
  class Any;
  class Raw;
  class Null;
  class Object;
  class Array;
  class Boolean;
  class Number;
  class String;

  using ObjectEntries = std::map<SSC::String, Any>;
  using ArrayEntries = Vector<Any>;

  enum class Type {
    Empty = -1,
    Any = 0,
    Null = 1,
    Object = 2,
    Array = 3,
    Boolean = 4,
    Number = 5,
    String = 6,
    Raw = 7,
    Error = 8
  };

  template <typename D, Type t> struct Value {
    public:
      Type type = t;
      D data;

      const SSC::String typeof () const {
        switch (this->type) {
          case Type::Empty: return SSC::String("empty");
          case Type::Raw: return SSC::String("raw");
          case Type::Any: return SSC::String("any");
          case Type::Array: return SSC::String("array");
          case Type::Boolean: return SSC::String("boolean");
          case Type::Number: return SSC::String("number");
          case Type::Null: return SSC::String("null");
          case Type::Object: return SSC::String("object");
          case Type::String: return SSC::String("string");
          case Type::Error: return SSC::String("error");
        }
      }

      bool isError () const { return this->type == Type::Error; }
      bool isRaw () const { return this->type == Type::Raw; }
      bool isArray () const { return this->type == Type::Array; }
      bool isBoolean () const { return this->type == Type::Boolean; }
      bool isNumber () const { return this->type == Type::Number; }
      bool isNull () const { return this->type == Type::Null; }
      bool isObject () const { return this->type == Type::Object; }
      bool isString () const { return this->type == Type::String; }
      bool isEmpty () const { return this->type == Type::Empty; }
  };

  class Error : public std::invalid_argument, public Value<SSC::String, Type::Error> {
    public:
      int code = 0;
      SSC::String name;
      SSC::String message;
      SSC::String location;

      Error () : std::invalid_argument("") {};
      Error (const Error& error) : std::invalid_argument(error.str()) {
        this->code = error.code;
        this->name = error.name;
        this->message = error.message;
        this->location = error.location;
      }

      Error (Error* error) : std::invalid_argument(error->str()) {
        this->code = error->code;
        this->name = error->name;
        this->message = error->message;
        this->location = error->location;
      }

      Error (
        const SSC::String& name,
        const SSC::String& message,
        int code = 0
      ) : std::invalid_argument(name + ": " + message) {
        this->name = name;
        this->code = code;
        this->message = message;
      }

      Error (
        const SSC::String& message
      ) : std::invalid_argument(message) {
        this->message = message;
      }

      Error (
        const SSC::String& name,
        const SSC::String& message,
        const SSC::String& location
      ) : std::invalid_argument(name + ": " + message + " (from " + location + ")") {
        this->name = name;
        this->message = message;
        this->location = location;
      }

      SSC::String value () const {
        return this->str();
      }

      const char* what () const noexcept override {
        return this->message.c_str();
      }

      const SSC::String str () const {
        if (this->name.size() > 0 && this->message.size() > 0 && this->location.size() > 0) {
          return this->name + ": " + this->message + " (from " + this->location + ")";
        } else if (this->name.size() > 0 && this->message.size() > 0) {
          return this->name + ": " + this->message;
        } else if (this->name.size() > 0 && this->location.size() > 0) {
          return this->name + " (from " + this->location + ")";
        } else if (this->message.size() > 0 && this->location.size() > 0) {
          return this->message + " (from " + this->location + ")";
        } else if (this->name.size() > 0) {
          return this->name;
        } else if (this->message.size() > 0) {
          return this->message;
        }

        return "";
      }
  };

  class Null : public Value<std::nullptr_t, Type::Null> {
    public:
      Null () {}
      Null (std::nullptr_t) : Null() {}
      std::nullptr_t value () const {
        return nullptr;
      }

      SSC::String str () const {
        return "null";
      }
  };

  extern Null null;

  class Any : public Value<void *, Type::Any> {
    public:
      SharedPointer<void> pointer = nullptr;

      Any () {
        this->pointer = nullptr;
        this->type = Type::Null;
      }

      Any (const Any& any) : pointer(any.pointer) {
        this->type = any.type;
      }

      Any (Type type, SharedPointer<void> pointer) : pointer(pointer) {
        this->type = type;
      }

      Any (std::nullptr_t);
      Any (const Null);
      Any (bool);
      Any (const Boolean);
      Any (int64_t);
      Any (uint64_t);
      Any (uint32_t);
      Any (int32_t);
      Any (double);
    #if SOCKET_RUNTIME_PLATFORM_APPLE
      Any (size_t);
      Any (ssize_t);
    #else
      Any (long long);
    #endif
      Any (const Number);
      Any (const char);
      Any (const char *);
      Any (const SSC::String);
      Any (const String);
      Any (const Object);
      Any (const ObjectEntries);
      Any (const Array);
      Any (const ArrayEntries);
      Any (const Raw);
      Any (const Error);
    #if SOCKET_RUNTIME_PLATFORM_APPLE
      Any (const NSError*);
    #elif SOCKET_RUNTIME_PLATFORM_LINUX
      Any (const GError*);
    #endif

      ~Any () {
        this->pointer = nullptr;
        this->type = Type::Any;
      }

      SSC::String str () const;

      template <typename T> T& as () const {
        auto ptr = this->pointer.get();

        if (ptr != nullptr && this->type != Type::Null) {
          return *reinterpret_cast<T *>(ptr);
        }

        throw Error("BadCastError", "cannot cast to null value", __PRETTY_FUNCTION__);
      }
  };

  class Raw : public Value<SSC::String, Type::Raw> {
    public:
      Raw (const Raw& raw) { this->data = raw.data; }
      Raw (const Raw* raw) { this->data = raw->data; }
      Raw (const SSC::String& source) { this->data = source; }

      const SSC::String str () const {
        return this->data;
      }
  };

  extern Any anyNull;

  inline const auto typeof (const Any& any) {
    return any.typeof();
  }

  class Object : public Value<ObjectEntries, Type::Object> {
    public:
      using Entries = ObjectEntries;
      Object () = default;
      Object (std::map<SSC::String, int> entries) {
        for (auto const &tuple : entries) {
          auto key = tuple.first;
          auto value = tuple.second;
          this->data.insert_or_assign(key, value);
        }
      }

      Object (std::map<SSC::String, bool> entries) {
        for (auto const &tuple : entries) {
          auto key = tuple.first;
          auto value = tuple.second;
          this->data.insert_or_assign(key, value);
        }
      }

      Object (std::map<SSC::String, double> entries) {
        for (auto const &tuple : entries) {
          auto key = tuple.first;
          auto value = tuple.second;
          this->data.insert_or_assign(key, value);
        }
      }

      Object (std::map<SSC::String, int64_t> entries) {
        for (auto const &tuple : entries) {
          auto key = tuple.first;
          auto value = tuple.second;
          this->data.insert_or_assign(key, value);
        }
      }

      Object (const Object::Entries entries) {
        for (const auto& tuple : entries) {
          auto key = tuple.first;
          auto value = tuple.second;
          this->data.insert_or_assign(key, value);
        }
      }

      Object (const Object& object) {
        this->data = object.value();
      }

      Object (const std::map<SSC::String, SSC::String> map) {
        for (const auto& tuple : map) {
          auto key = tuple.first;
          auto value = Any(tuple.second);
          this->data.insert_or_assign(key, value);
        }
      }

      Object (const Error& error) {
        if (error.name.size() > 0) {
          this->set("name", error.name);
        }

        if (error.message.size() > 0) {
          this->set("message", error.message);
        }

        if (error.location.size() > 0) {
          this->set("location", error.location);
        }

        if (error.code != 0) {
          this->set("code", error.code);
        }
      }

      SSC::String str () const;

      const Object::Entries value () const {
        return this->data;
      }

      Any& get (const SSC::String key) {
        if (this->data.find(key) != this->data.end()) {
          return this->data.at(key);
        }

        return anyNull;
      }

      void set (const SSC::String key, Any value) {
        this->data[key] = value;
      }

      bool has (const SSC::String& key) const {
        return this->data.find(key) != this->data.end();
      }

      Any operator [] (const SSC::String& key) const {
        if (this->data.find(key) != this->data.end()) {
          return this->data.at(key);
        }

        return nullptr;
      }

      Any &operator [] (const SSC::String& key) {
        return this->data[key];
      }

      auto size () const {
        return this->data.size();
      }
  };

  class Array : public Value<ArrayEntries, Type::Array> {
    public:
      using Entries = ArrayEntries;
      Array () = default;
      Array (const Array& array) {
        this->data = array.value();
      }

      Array (const Array::Entries entries) {
        for (const auto& value : entries) {
          this->data.push_back(value);
        }
      }

      SSC::String str () const;

      Array::Entries value () const {
        return this->data;
      }

      bool has (const unsigned int index) const {
        return this->data.size() < index;
      }

      auto size () const {
        return this->data.size();
      }

      Any get (const unsigned int index) const {
        if (index < this->data.size()) {
          return this->data.at(index);
        }

        return nullptr;
      }

      void set (const unsigned int index, Any value) {
        if (index >= this->data.size()) {
          this->data.resize(index + 1);
        }

        this->data[index] = value;
      }

      void push (Any value) {
        this->set(this->size(), value);
      }

      Any& pop () {
        if (this->size() == 0) {
          return anyNull;
        }

        auto& value = this->data.back();
        this->data.pop_back();
        return value;
      }

      Any operator [] (const unsigned int index) const {
        if (index >= this->data.size()) {
          return nullptr;
        }

        return this->data.at(index);
      }

      Any &operator [] (const unsigned int index) {
        if (index >= this->data.size()) {
          this->data.resize(index + 1);
        }

        return this->data.at(index);
      }
  };

  class Boolean : public Value<bool, Type::Boolean> {
    public:
      Boolean () = default;
      Boolean (const Boolean& boolean) {
        this->data = boolean.value();
      }

      Boolean (bool boolean) {
        this->data = boolean;
      }

      Boolean (int data) {
        this->data = data != 0;
      }

      Boolean (int64_t data) {
        this->data = data != 0;
      }

      Boolean (double data) {
        this->data = data != 0;
      }

      Boolean (void *data) {
        this->data = data != nullptr;
      }

      Boolean (SSC::String string) {
        this->data = string.size() > 0;
      }

      bool value () const {
        return this->data;
      }

      SSC::String str () const {
        return this->data ? "true" : "false";
      }
  };

  class Number : public Value<double, Type::Number> {
    public:
      Number () = default;
      Number (const Number& number) {
        this->data = number.value();
      }

      Number (double number) {
        this->data = number;
      }

      Number (char number) {
        this->data = (double) number;
      }

      Number (int number) {
        this->data = (double) number;
      }

      Number (int64_t number) {
        this->data = (double) number;
      }

      Number (bool number) {
        this->data = (double) number;
      }

      Number (const String& string);

      float value () const {
        return this->data;
      }

      SSC::String str () const;
  };

  class String : public Value<SSC::String, Type::String> {
    public:
      String () = default;
      String (const String& data) {
        this->data = SSC::String(data.value());
      }

      String (const SSC::String data) {
        this->data = data;
      }

      String (const char data) {
        this->data = SSC::String(1, data);
      }

      String (const char *data) {
        this->data = SSC::String(data);
      }

      String (const Any& any) {
        this->data = any.str();
      }

      String (const Number& number);

      String (const Boolean& boolean) {
        this->data = boolean.str();
      }

      String (const Error& error) {
        this->data = error.str();
      }

      SSC::String str () const;

      SSC::String value () const {
        return this->data;
      }

      auto size () const {
        return this->data.size();
      }
  };
}

#endif
