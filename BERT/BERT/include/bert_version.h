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
#pragma once

// the release number. Install/build-installer.ps1 reads this line to name
// the installer, so keep the define on one line in this form.
#define BERT_VERSION_NUMBER L"2.4.3"

// a tag for builds from this branch, and the time the add-in was compiled,
// so a locally built add-in can be told from a release in the console's
// Help menu. the stamp is the compile time of bert.cc, the one file that
// includes this header, so it only moves when that file recompiles.
#define BERT_VERSION_TAG L"-r19"

#define BERT_WIDEN_(s) L ## s
#define BERT_WIDEN(s) BERT_WIDEN_(s)

#define BERT_VERSION \
  BERT_VERSION_NUMBER BERT_VERSION_TAG \
  L" (built " BERT_WIDEN(__DATE__) L" " BERT_WIDEN(__TIME__) L")"
