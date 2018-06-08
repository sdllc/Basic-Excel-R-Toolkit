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

#include <string>
#include <vector>
#include <inttypes.h>
#include "json11/json11.hpp"

/** moving to config, struct for now */
class LanguageDescriptor {
public:
  std::string name_;
  std::string executable_;
  std::string prefix_;
  std::string tag_;
  std::vector<std::string> extensions_;
  std::string command_arguments_;
  std::string prepend_path_;
  uint32_t startup_resource_;
  std::string startup_resource_path_;
  std::string home_;

  std::vector < std::string > home_candidates_;

  bool named_arguments_;
  int32_t priority_;

public:
  LanguageDescriptor(
    const std::string &name,
    const std::string &executable,
    const std::string &prefix,
    const std::vector<std::string> &extensions,
    const std::string &command_arguments,
    const std::string &prepend_path,
    const std::string &home,
    uint32_t startup_resource,
    const std::string &startup_resource_path = "",
    const bool named_arguments = false
  ) 
    : name_(name)
    , executable_(executable)
    , prefix_(prefix)
    , extensions_(extensions)
    , command_arguments_(command_arguments)
    , prepend_path_(prepend_path)
    , home_(home)
    , startup_resource_(startup_resource)
    , startup_resource_path_(startup_resource_path)
    , named_arguments_(named_arguments)
  {}

  LanguageDescriptor() : priority_(0), named_arguments_(false) {}

  LanguageDescriptor(const LanguageDescriptor &rhs)
    : name_(rhs.name_)
    , executable_(rhs.executable_)
    , prefix_(rhs.prefix_)
    , priority_(rhs.priority_)
    , tag_(rhs.tag_)
    , extensions_(rhs.extensions_)
    , command_arguments_(rhs.command_arguments_)
    , prepend_path_(rhs.prepend_path_)
    , home_(rhs.home_)
    , startup_resource_(rhs.startup_resource_)
    , startup_resource_path_(rhs.startup_resource_path_)
    , named_arguments_(rhs.named_arguments_)
    , home_candidates_(rhs.home_candidates_)
  {}

public:

  /**
   * copies values from passed JSON object to language descriptor.
   * this is designed to support overlay from version, so that 
   * we can overlay as much or as little data as necessary/useful.
   */
  void FromJSON(const json11::Json& json, const std::string &home_directory);

};

