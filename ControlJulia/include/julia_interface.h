#pragma once

void JuliaInit();
void JuliaShutdown();
// void julia_exec();

void julia_exec_command(const std::string &command);

void JuliaExec(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);
void JuliaCall(BERTBuffers::CallResponse &response, const BERTBuffers::CallResponse &call);

