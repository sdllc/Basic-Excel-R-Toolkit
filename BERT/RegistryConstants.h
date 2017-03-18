/*
 * Basic Excel R Toolkit (BERT)
 * Copyright (C) 2014-2017 Structured Data, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef __REGISTRY_CONSTANTS_H
#define __REGISTRY_CONSTANTS_H

#define REGISTRY_KEY					"Software\\BERT"
#define REGISTRY_VALUE_ENVIRONMENT		"Environment"
#define REGISTRY_VALUE_R_USER			"R_USER"
#define REGISTRY_VALUE_R_HOME			"R_HOME"
#define REGISTRY_VALUE_STARTUP			"StartupFile"
#define REGISTRY_VALUE_FUNCTIONS_DIR	"FunctionsDir"
#define REGISTRY_VALUE_HIDE_MENU		"HideMenu"
#define REGISTRY_VALUE_INSTALL_DIR		"InstallDir"
#define REGISTRY_VALUE_PRESERVE_ENV		"PreserveEnvironment"

#define REGISTRY_VALUE_PIPE_OVERRIDE	"OverridePipeName"

#define DEFAULT_ENVIRONMENT				""
#define DEFAULT_R_USER					"%APPDATA%\\BERT"
#define DEFAULT_R_HOME					"%APPDATA%\\BERT\\R-3.2.1"
#define DEFAULT_R_STARTUP				"Functions.R"
#define DEFAULT_R_PRESERVE_ENV			0

#define DEFAULT_R_FUNCTIONS_DIR			"functions"

#define DEFAULT_CONSOLE_WIDTH			80
#define DEFAULT_CONSOLE_AUTO_WIDTH		1

// registry cache for update check -- use if-modified-since header

#define REGISTRY_VALUE_UPDATE_LAST_MODIFIED		"UpdateLastModified"
#define REGISTRY_VALUE_UPDATE_LAST_TAG			"UpdateLastTag"

#endif // #ifndef __REGISTRY_CONSTANTS_H


