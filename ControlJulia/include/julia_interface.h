#pragma once

void JuliaInit();
void JuliaShutdown();
// void julia_exec();

typedef enum {
  Error = 0,
  Success,
  Incomplete
} 
ExecResult;

ExecResult JuliaShellExec(const std::string &command, const std::string &shell_buffer);

void ListScriptFunctions(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);

void JuliaExec(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);
void JuliaCall(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);
void JuliaPostInit(BERTBuffers::CallResponse &response, BERTBuffers::CallResponse &translated_call);

bool ReadSourceFile(const std::string &file);
