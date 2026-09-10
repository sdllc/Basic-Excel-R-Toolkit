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

import * as remote from '@electron/remote';

/**
 * text clipboard for the renderer.
 *
 * electron no longer offers its clipboard module to renderer processes and
 * points at the standard web API instead, which is what this uses. that API
 * refuses to work while the document does not have the focus, which can
 * happen around native menus, so the main process clipboard (reached
 * through @electron/remote) is the fallback: it is the same clipboard, and
 * it does not care about focus.
 */
export const clipboard = {

  async writeText(text: string): Promise<void> {
    try {
      await navigator.clipboard.writeText(text);
    }
    catch (e) {
      console.info("clipboard write via the main process:", e.message || e);
      remote.clipboard.writeText(text);
    }
  },

  async readText(): Promise<string> {
    try {
      return await navigator.clipboard.readText();
    }
    catch (e) {
      console.info("clipboard read via the main process:", e.message || e);
      return remote.clipboard.readText();
    }
  }

};
