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
 
#ifndef __CONSOLE_GRAPHICS_DEVICE_H
#define __CONSOLE_GRAPHICS_DEVICE_H

#include <Rinternals.h>

namespace ConsoleGraphicsDevice {

  SEXP CreateConsoleDevice(const std::string &background, double width, double height, double pointsize, const std::string &type, void * pointer);

};

#endif // #ifndef __CONSOLE_GRAPHICS_DEVICE_H

