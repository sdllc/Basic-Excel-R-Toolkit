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

#include "stdafx.h"
#include "language_desc.h"

void LanguageDescriptor::FromJSON(const json11::Json& item, const std::string &home_directory) {

  if (item["extensions"].is_array()) {
    std::vector<std::string> extensions;
    for (const auto &extension : item["extensions"].array_items()) {
      extensions.push_back(extension.string_value());
    }
    extensions_ = extensions;
  }

  if (!item["startup_resource"].is_null()) {
    std::string startup_resource_path = home_directory;
    startup_resource_path.append("startup\\");
    startup_resource_path.append(item["startup_resource"].string_value());
    startup_resource_path_ = startup_resource_path;
  }

  // can probably just call bool_value() regardless, should return false if not present
  if (!item["named_arguments"].is_null()) named_arguments_ = item["named_arguments"].bool_value();

  if (!item["name"].is_null()) name_ = item["name"].string_value();
  if (!item["executable"].is_null()) executable_ = item["executable"].string_value();
  if (!item["prefix"].is_null()) prefix_ = item["prefix"].string_value();

  if (!item["priority"].is_null()) priority_ = item["priority"].int_value();
  if (!item["tag"].is_null()) tag_ = item["tag"].string_value();

  if (!item["command_arguments"].is_null()) command_arguments_ = item["command_arguments"].string_value();
  if (!item["prepend_path"].is_null()) prepend_path_ = item["prepend_path"].string_value();

  if (item["home"].is_array()) {
    for (auto candidate : item["home"].array_items()) {
      if(candidate.is_string()) home_candidates_.push_back(candidate.string_value());
    }
  }
  else if (!item["home"].is_null()) home_ = item["home"].string_value();

}
