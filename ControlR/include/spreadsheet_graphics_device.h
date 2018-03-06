
#ifndef __SPREADSGEET_GRAPHICS_DEVICE_H
#define __SPREADSGEET_GRAPHICS_DEVICE_H

#include <Rinternals.h>

SEXP CreateSpreadsheetDevice(const std::string &background, double width, double height, double pointsize, const std::string &name, void * pointer);

#endif // #ifndef __SPREADSGEET_GRAPHICS_DEVICE_H

