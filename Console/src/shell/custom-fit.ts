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

/// <reference path="../../node_modules/xterm/typings/xterm.d.ts"/>

import { Terminal } from 'xterm';

export interface IGeometry {
  rows: number;
  cols: number;
}

export function proposeGeometry(term: Terminal): IGeometry {
  if (!term.element.parentElement) {
    return null;
  }
  const parentElementStyle = window.getComputedStyle(term.element.parentElement);
  const parentElementHeight = parseInt(parentElementStyle.getPropertyValue('height'));
  const parentElementWidth = Math.max(0, parseInt(parentElementStyle.getPropertyValue('width')));
  const elementStyle = window.getComputedStyle(term.element);
  const elementPadding = {
    top: parseInt(elementStyle.getPropertyValue('padding-top')),
    bottom: parseInt(elementStyle.getPropertyValue('padding-bottom')),
    right: parseInt(elementStyle.getPropertyValue('padding-right')),
    left: parseInt(elementStyle.getPropertyValue('padding-left'))
  };
  const elementPaddingVer = elementPadding.top + elementPadding.bottom;
  const elementPaddingHor = elementPadding.right + elementPadding.left;
//  const availableHeight = parentElementHeight - elementPaddingVer;
  const availableHeight = parentElementHeight - elementPaddingVer - (<any>term).viewport.scrollBarWidth;
  const availableWidth = parentElementWidth - elementPaddingHor - (<any>term).viewport.scrollBarWidth;
  const geometry = {
    cols: Math.floor(availableWidth / (<any>term).renderer.dimensions.actualCellWidth),
    rows: Math.floor(availableHeight / (<any>term).renderer.dimensions.actualCellHeight)
  };

  return geometry;
}

export function fit(term: Terminal, minimum_columns = 0): void {

  if(!term['custom_fit_listener_attached']){
    term['custom_fit_listener_attached'] = true;
    term['viewport'].viewportElement.addEventListener('scroll', e => {
      term['screenElement'].style.left = (-term['viewport'].viewportElement.scrollLeft) + "px";
    });
  }

  const geometry = proposeGeometry(term);
  geometry.cols = Math.max(geometry.cols, minimum_columns);
  if (geometry) {
    // Force a full render
    if (term.rows !== geometry.rows || term.cols !== geometry.cols) {
      (<any>term).renderer.clear();

      // we don't want to size the buffer based on target width, we want
      // to use max characters width (if it's wider) -- this can happen if 
      // you resize without resetting. unfortunately these buffers are pre-
      // allocated.

      let max = geometry.cols - 2; // if it's wider
      term['buffers']._activeBuffer.lines.forEach(line => {
        let line_max = max;
        for( let i = max; i< line.length; i++ ){
          if(line[i][0] !== 131840) line_max = i;
        }
        max = Math.max(max, line_max);
      });

      let dimensions = term['renderer'].dimensions;

      let cols = Math.max(geometry.cols, max + 2);
      term.resize(cols, geometry.rows);

      term['viewport'].scrollArea.style.width = dimensions.canvasWidth + "px";

    }
  }
}

export function apply(terminalConstructor: typeof Terminal): void {
  (<any>terminalConstructor.prototype).proposeGeometry = function (): IGeometry {
    return proposeGeometry(this);
  };

  (<any>terminalConstructor.prototype).fit = function (minimum_columns = 0): void {
    fit(this, minimum_columns);
  };

}
