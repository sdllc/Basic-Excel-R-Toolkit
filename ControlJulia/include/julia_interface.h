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

ExecResult julia_exec_command(const std::string &command, const std::vector<std::string> &shell_buffer);

void JuliaExec(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);
void JuliaCall(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);

bool ReadSourceFile(const std::string &file);
