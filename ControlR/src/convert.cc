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

#include "convert.h"

/**
 * thanks to
 * http://www.zedwood.com/article/cpp-is-valid-utf8-string-function
 */
bool ValidUTF8(const char *string, int len) {

  if (len <= 0) len = strlen(string);

  int c, i, ix, n, j;
  for (i = 0, ix = len; i < ix; i++)
  {
    c = (unsigned char)string[i];
    //if (c==0x09 || c==0x0a || c==0x0d || (0x20 <= c && c <= 0x7e) ) n = 0; // is_printable_ascii
    if (0x00 <= c && c <= 0x7f) n = 0; // 0bbbbbbb
    else if ((c & 0xE0) == 0xC0) n = 1; // 110bbbbb
    else if (c == 0xed && i<(ix - 1) && ((unsigned char)string[i + 1] & 0xa0) == 0xa0) return false; //U+d800 to U+dfff
    else if ((c & 0xF0) == 0xE0) n = 2; // 1110bbbb
    else if ((c & 0xF8) == 0xF0) n = 3; // 11110bbb
                                        //else if (($c & 0xFC) == 0xF8) n=4; // 111110bb //byte 5, unnecessary in 4 byte UTF-8
                                        //else if (($c & 0xFE) == 0xFC) n=5; // 1111110b //byte 6, unnecessary in 4 byte UTF-8
    else return false;
    for (j = 0; j<n && i<ix; j++) { // n bytes matching 10bbbbbb follow ?
      if ((++i == ix) || (((unsigned char)string[i] & 0xC0) != 0x80))
        return false;
    }
  }
  return true;
}

/**
 * convert. see docs elsewhere on rationale. this method owns
 * the output buffer; copy if you need to preserve it.
 */
void WindowsCPToUTF8(const char *input_buffer, int input_length, char **output_buffer, int *output_length){

  const int32_t chunk = 256; // 32;

  static char *narrow_string = new char[chunk];
  static uint32_t narrow_string_length = chunk;

  static WCHAR *wide_string = new WCHAR[chunk];
  static uint32_t wide_string_length = chunk;
  
  // stats? bad guesses?

  // we can probably use fixed buffers here and expand, on the theory
  // that they won't get too large... if anything most console messages are 
  // too small, we should do some buffering (not here)

  int wide_size = MultiByteToWideChar(CP_ACP, MB_COMPOSITE, input_buffer, input_length, 0, 0);
  if (wide_size >= wide_string_length) {
    wide_string_length = chunk * (1 + (wide_size / chunk));
    delete[] wide_string;
    wide_string = new WCHAR[wide_string_length];
  }
  MultiByteToWideChar(CP_ACP, MB_COMPOSITE, input_buffer, input_length, wide_string, wide_string_length);
  wide_string[wide_size] = 0;

  int narrow_size = WideCharToMultiByte(CP_UTF8, 0, wide_string, wide_size, 0, 0, 0, 0);
  if (narrow_size >= narrow_string_length) {
    narrow_string_length = chunk * (1 + (narrow_size / chunk));
    delete[] narrow_string;
    narrow_string = new char[narrow_string_length];
  }
  WideCharToMultiByte(CP_UTF8, 0, wide_string, wide_size, narrow_string, narrow_string_length, 0, 0);
  narrow_string[narrow_size] = 0;

  *output_buffer = narrow_string;
  *output_length = narrow_size;

}

/**
 * alloc-heavy version
 */
std::string WindowsCPToUTF8_2(const char *input_buffer, int input_length) {
  
  if (input_length <= 0) input_length = strlen(input_buffer);

  int wide_size = MultiByteToWideChar(CP_ACP, MB_COMPOSITE, input_buffer, input_length, 0, 0);
  std::wstring wide_string(wide_size, '\0');
  MultiByteToWideChar(CP_ACP, MB_COMPOSITE, input_buffer, input_length, &(wide_string[0]), wide_size);

  int narrow_size = WideCharToMultiByte(CP_UTF8, 0, &(wide_string[0]), wide_size, 0, 0, 0, 0);
  std::string narrow_string(narrow_size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, &(wide_string[0]), wide_size, &(narrow_string[0]), narrow_size, 0, 0);

  return narrow_string;
}
