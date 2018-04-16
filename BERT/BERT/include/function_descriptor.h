/**
 * Copyright (c) 2017-2018 Structured Data, LLC
 * 
 * This file is part of BERT.
 *
 * BERT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BERT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BERT.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

// fwd
class LanguageService;

/**
 * class representing a function argument: name, description, default value.
 * FIXME: type?
 */
class ArgumentDescriptor {
public:
  std::string name_;
  std::string description_;
  std::string default_value_; // not necessarily a string, this is just a representation

public:
  ArgumentDescriptor(const std::string &name = "", const std::string &default_value = "", const std::string &description = "")
    : name_(name)
    , description_(description)
    , default_value_(default_value) {}

  ArgumentDescriptor(const ArgumentDescriptor &rhs) {
    name_ = rhs.name_;
    description_ = rhs.description_;
    default_value_ = rhs.default_value_;
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

  /* 
   * optional alias. if there's an alias, this will be the value used in the Excel function.
   * the name value (not the alias) will get passed back to the language service. if alias
   * is empty, then name will be used for both (as now).
   */
  std::string alias_;

  std::string name_;

  std::string category_;

  std::string description_;
  
  /** it's a waste to carry this around, but it seems convenient */
  // std::string language_prefix_;

  /** since we have the key, and prefix is a one-time lookup, create a lookup */
  uint32_t language_key_;

  /** for flush lookup. FIXME: merge or remove the two keys */
  std::string language_name_;

  ARGUMENT_LIST arguments_;

  std::shared_ptr<LanguageService> language_service_;

  /** 
   * flags is an opaque field, for use by languages. it is currently 
   * intended for R to indicate remapped functions. there are supporting fields in 
   * PB function descriptors and function calls.
   */
  uint32_t flags_;

  /**
   * this ID is assigned when we call xlfRegister, and we need to keep
   * it around to call xlfUnregister if we are rebuilding the functions.
   * (excel uses num (i.e. double) for this)
   */
  double register_id_;

public:
  FunctionDescriptor(const std::string &name, const std::string &alias, const std::string &language_name, uint32_t language_key, const std::string &category = "", const std::string &description = "", const ARGUMENT_LIST &args = {}, uint32_t flags = 0, std::shared_ptr<LanguageService> language_service = 0)
    : name_(name)
    , alias_(alias)
    , language_name_(language_name)
    , language_key_(language_key)
    , category_(category)
    , description_(description)
    , flags_(flags)
    , register_id_(0)
    , language_service_(language_service)
  {
    for (auto arg : args) arguments_.push_back(arg);
  }

  ~FunctionDescriptor() {
    // ...
  }

  FunctionDescriptor(const FunctionDescriptor &rhs) {
    name_ = rhs.name_;
    alias_ = rhs.alias_;
    language_name_ = rhs.language_name_;
    category_ = rhs.category_;
    flags_ = rhs.flags_;
    description_ = rhs.description_;
    register_id_ = rhs.register_id_;
    language_key_ = rhs.language_key_;
    language_service_ = rhs.language_service_;
    for (auto arg : rhs.arguments_) arguments_.push_back(arg);
  }
};

typedef std::vector<std::shared_ptr<FunctionDescriptor>> FUNCTION_LIST;

