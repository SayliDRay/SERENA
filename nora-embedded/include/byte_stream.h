#pragma once
#include <stddef.h>
namespace nora {
// Hardware UART or a scripted host test; no Arduino dependency in the adapter.
class ByteStream {
 public:
  virtual ~ByteStream() {}
  virtual int read() = 0; // -1 if no byte is available
  virtual void write(const char* text) = 0;
};
}
