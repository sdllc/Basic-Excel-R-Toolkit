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
