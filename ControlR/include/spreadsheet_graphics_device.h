
#ifndef __SPREADSGEET_GRAPHICS_DEVICE_H
#define __SPREADSGEET_GRAPHICS_DEVICE_H

#include <Rinternals.h>
#include "gdi_graphics_device.h"

namespace SpreadsheetGraphicsDevice {

  SEXP CreateSpreadsheetDevice(const std::string &name, const std::string &background, double width, double height, double pointsize, void * pointer);
  std::vector<gdi_graphics_device::Device*> UpdatePendingGraphics();

};



#endif // #ifndef __SPREADSGEET_GRAPHICS_DEVICE_H

