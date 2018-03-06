
#ifndef __CONSOLE_GRAPHICS_DEVICE_H
#define __CONSOLE_GRAPHICS_DEVICE_H

#include <Rinternals.h>

namespace ConsoleGraphicsDevice {

  SEXP CreateConsoleDevice(const std::string &background, double width, double height, double pointsize, const std::string &type, void * pointer);

};

#endif // #ifndef __CONSOLE_GRAPHICS_DEVICE_H

