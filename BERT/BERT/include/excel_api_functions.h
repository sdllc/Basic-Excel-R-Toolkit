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

/** */
void RegisterFunctions();

/**
 * removes registered functions. this should be called before re-registering.
 *
 * FIXME: if it's safe to call this with no registered functions, then perhaps
 * it should just be rolled in to the register function.
 */
void UnregisterFunctions(const std::string &language = "");

/** 
 * registers the "BERT.Exec.X" and "BERT.Call.X" functions for a language
 */
bool ExcelRegisterLanguageCalls(const char *language_name, uint32_t language_key, bool generic = false);

