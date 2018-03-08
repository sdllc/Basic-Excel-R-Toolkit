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
#include <stdint.h>
#include <string>

/**
 * thanks to
 * http://www.zedwood.com/article/cpp-is-valid-utf8-string-function
 */
bool ValidUTF8(const char *string, int len);

void WindowsCPToUTF8(const char *input_buffer, int input_length, char **output_buffer, int *output_length);

std::string WindowsCPToUTF8_2(const char *input_buffer, int input_length);
