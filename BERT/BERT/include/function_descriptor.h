#pragma once

/**
 * class representing a function argument: name, description, default value.
 * FIXME: type?
 */
class ArgumentDescriptor {
public:
  std::string name;
  std::string description;
  std::string default_value; // not necessarily a string, this is just a representation

public:
  ArgumentDescriptor(const std::string &name = "", const std::string &default_value = "", const std::string &description = "")
    :name(name)
    , description(description)
    , default_value(default_value) {}

  ArgumentDescriptor(const ArgumentDescriptor &rhs) {
    name = rhs.name;
    description = rhs.description;
    default_value = rhs.default_value;
  }
};

/** type: list of arguments */
typedef std::vector<std::shared_ptr<ArgumentDescriptor>> ARGUMENT_LIST;

/**
 * class representing a function call: name, metadata (category/description), list of arguments.
 * FIXME: return type?
 */
class FunctionDescriptor {

public:
  std::string name;
  std::string category;
  std::string description;
  
  /** it's a waste to carry this around, but it seems convenient */
  // std::string language_prefix_;

  /** since we have the key, and prefix is a one-time lookup, create a lookup */
  uint32_t language_key_;

  ARGUMENT_LIST arguments;

  /**
   * this ID is assigned when we call xlfRegister, and we need to keep
   * it around to call xlfUnregister if we are rebuilding the functions.
   */
  int32_t register_id;

public:
  FunctionDescriptor(const std::string &name, uint32_t language_key, const std::string &category = "", const std::string &description = "", const ARGUMENT_LIST &args = {})
    : name(name)
    , language_key_(language_key)
    , category(category)
    , description(description)
    , register_id(0)
  {
    for (auto arg : args) arguments.push_back(arg);
  }

  ~FunctionDescriptor() {
    // ...
  }

  FunctionDescriptor(const FunctionDescriptor &rhs) {
    name = rhs.name;
    category = rhs.category;
    description = rhs.description;
    register_id = rhs.register_id;
    language_key_ = rhs.language_key_;
    for (auto arg : rhs.arguments) arguments.push_back(arg);
  }
};

typedef std::vector<std::shared_ptr<FunctionDescriptor>> FUNCTION_LIST;

