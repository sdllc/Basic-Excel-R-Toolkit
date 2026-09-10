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
 * the few things the terminal needs from xterm that its public API does
 * not offer, gathered in one place. each is a public API call where one
 * exists, and otherwise a single reach into xterm's internal _core object
 * with a fallback, so that an xterm upgrade has one file to check.
 */

import { Terminal } from '@xterm/xterm';

export interface CellDimensions {
  width: number;
  height: number;
}

export interface CursorClientRect {
  left: number;
  right: number;
  top: number;
  bottom: number;
}

/** size of one character cell, in css pixels */
export function GetCellDimensions(terminal: Terminal): CellDimensions {

  const core = (terminal as any)._core;
  const cell = core && core._renderService && core._renderService.dimensions
    && core._renderService.dimensions.css && core._renderService.dimensions.css.cell;

  if (cell && cell.width && cell.height) {
    return { width: cell.width, height: cell.height };
  }

  // fallback: measure a rendered row

  const row = terminal.element ? terminal.element.querySelector('.xterm-rows > div') as HTMLElement : null;
  if (row) {
    const rect = row.getBoundingClientRect();
    return { width: rect.width / terminal.cols, height: rect.height };
  }

  return { width: 0, height: 0 };
}

/** the element the character cells are drawn in */
export function GetScreenElement(terminal: Terminal): HTMLElement {
  return terminal.element ? terminal.element.querySelector('.xterm-screen') as HTMLElement : null;
}

/**
 * client rect of the cursor cell, shifted by a number of columns. used to
 * position tooltips and the autocomplete list.
 */
export function GetCursorClientRect(terminal: Terminal, offset_x = 0): CursorClientRect {

  const buffer = terminal.buffer.active;
  const cell = GetCellDimensions(terminal);
  const origin = (GetScreenElement(terminal) || terminal.element).getBoundingClientRect();

  const x = buffer.cursorX + offset_x;
  const y = buffer.cursorY;

  return {
    left: origin.left + x * cell.width,
    right: origin.left + (x + 1) * cell.width,
    top: origin.top + y * cell.height,
    bottom: origin.top + (y + 1) * cell.height
  };
}

/**
 * calls handler when lines are dropped from the top of the scrollback,
 * which shifts every buffer line index up by that many. there is no public
 * event for this; the buffer's line list raises one internally. returns
 * false if that event could not be found.
 */
export function OnBufferTrim(terminal: Terminal, handler: (count: number) => void): boolean {
  const core = (terminal as any)._core;
  const lines = core && core.buffer && core.buffer.lines;
  if (lines && typeof lines.onTrim === 'function') {
    lines.onTrim(handler);
    return true;
  }
  console.warn("xterm buffer trim event not found; annotations will drift once the scrollback fills");
  return false;
}
