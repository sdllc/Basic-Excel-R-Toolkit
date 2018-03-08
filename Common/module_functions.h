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

#include <Windows.h>
#include <Shlwapi.h>

#include <string>
#include <vector>

#define DEFAULT_BASE_KEY        HKEY_CURRENT_USER
#define DEFAULT_REGISTRY_KEY    "Software\\BERT2"

#define PATH_PATH_SEPARATOR ";"

namespace ModuleFunctions {

  /** reads resource in this dll */
  std::string ReadResource(LPTSTR resource_id);

  /** get the path of the current module (not containing process) */
  std::string ModulePath();

}
