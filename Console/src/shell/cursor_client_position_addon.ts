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

/** 
 * cursor position locator, implemented as terminal add-on. not
 * in love with patching prototype, but should be fine for electron
 */
const GetCursorPosition = function(terminal, offset_x = 0){

  // character position
  let x = terminal.buffer.x;
  let y = terminal.buffer.y;

  // renderer dimensions
  let dimensions = terminal.renderer.dimensions;

  // position is relative to the canvas... although we should be
  // able to use the container node as a proxy. fixme: cache, or
  // use our node? (actually this is our node, we should just cache)
  let client_rect = (terminal.parent as HTMLElement).getBoundingClientRect();
  
  // TAG: switching scaled -> actual to fix highdpi
  
  let rect = {
    left: client_rect.left + ((x+offset_x) * dimensions.actualCellWidth),
    right: client_rect.left + ((x+offset_x+1) * dimensions.actualCellWidth),
    top: client_rect.top + (y * dimensions.actualCellHeight),
    bottom: client_rect.top + ((y+1) * dimensions.actualCellHeight),
  };
  
  return rect;

}

export const apply = function(terminalConstructor) {
  terminalConstructor.prototype.GetCursorPosition = function (offset_x = 0) {
    return GetCursorPosition(this, offset_x); // this will be terminal
  };
}
