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

import wcwidth_string from 'wcwidth';

/**
 * display width, in cells, of a single UTF-16 code unit. xterm 3 exported
 * this from its own character width table; current versions do not, so it
 * comes from the wcwidth package instead.
 */
export function wcwidth(code: number): number {
  return wcwidth_string(String.fromCharCode(code));
}
