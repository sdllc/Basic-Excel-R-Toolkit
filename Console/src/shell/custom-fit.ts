/**
 * original license (fit)
 * 
 * Copyright (c) 2014 The xterm.js authors. All rights reserved.
 * @license MIT
 *
 * Fit terminal columns and rows to the dimensions of its DOM element.
 *
 * ## Approach
 *
 *    Rows: Truncate the division of the terminal parent element height by the
 *          terminal row height.
 * Columns: Truncate the division of the terminal parent element width by the
 *          terminal character width (apply display: inline at the terminal
 *          row and truncate its width with the current number of columns).
 * 
 * 
 */

import { Terminal } from '@xterm/xterm';
import { FitAddon } from '@xterm/addon-fit';

/**
 * fits the terminal to its container, like the fit addon, except that the
 * column count never drops below the widest line in the buffer (plus a
 * margin). wide output then scrolls horizontally instead of rewrapping;
 * the terminal's container is what scrolls (see TerminalImplementation).
 */
export function fit(terminal: Terminal, fit_addon: FitAddon, minimum_columns = 0): void {

  const geometry = fit_addon.proposeDimensions();
  if (!geometry) return;

  let cols = Math.max(geometry.cols, minimum_columns);

  const buffer = terminal.buffer.active;
  let widest = 0;
  for (let i = 0; i < buffer.length; i++) {
    const line = buffer.getLine(i);
    if (line) widest = Math.max(widest, line.translateToString(true).length);
  }
  cols = Math.max(cols, widest + 2);

  if (terminal.rows !== geometry.rows || terminal.cols !== cols) {
    terminal.resize(cols, geometry.rows);
  }

}
