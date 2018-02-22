
#pragma once

#include <string>
#include <vector>
#include <inttypes.h>

/** moving to config, struct for now */
class LanguageDescriptor {
public:
  std::string name_;
  std::string executable_;
  std::string prefix_;
  std::vector<std::string> extensions_;
  std::string command_arguments_;
  std::string prepend_path_;
  uint32_t startup_resource_;

public:
  LanguageDescriptor(
    const std::string &name,
    const std::string &executable,
    const std::string &prefix,
    const std::vector<std::string> &extensions,
    const std::string &command_arguments,
    const std::string &prepend_path,
    uint32_t startup_resource
  ) 
    : name_(name)
    , executable_(executable)
    , prefix_(prefix)
    , extensions_(extensions)
    , command_arguments_(command_arguments)
    , prepend_path_(prepend_path)
    , startup_resource_(startup_resource)
  {}

  LanguageDescriptor(const LanguageDescriptor &rhs)
    : name_(rhs.name_)
    , executable_(rhs.executable_)
    , prefix_(rhs.prefix_)
    , extensions_(rhs.extensions_)
    , command_arguments_(rhs.command_arguments_)
    , prepend_path_(rhs.prepend_path_)
    , startup_resource_(rhs.startup_resource_)
  {}

};

