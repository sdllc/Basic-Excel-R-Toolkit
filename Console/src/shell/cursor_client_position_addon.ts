
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
