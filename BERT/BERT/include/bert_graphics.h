#pragma once

namespace BERTGraphics {
  
  void UpdateGraphics(const BERTBuffers::GraphicsUpdate &graphics, LPDISPATCH application_dispatch);

  bool QuerySize(const std::string &name, BERTBuffers::CallResponse &response, LPDISPATCH application_dispatch);

}