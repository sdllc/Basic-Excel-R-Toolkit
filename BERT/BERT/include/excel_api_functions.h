#pragma once

/** */
void RegisterFunctions();

/**
 * removes registered functions. this should be called before re-registering.
 *
 * FIXME: if it's safe to call this with no registered functions, then perhaps
 * it should just be rolled in to the register function.
 */
void UnregisterFunctions();

/** 
 * registers the "BERT.Exec.X" and "BERT.Call.X" functions for a language
 */
bool ExcelRegisterLanguageCalls(const char *language_name, uint32_t language_key);

