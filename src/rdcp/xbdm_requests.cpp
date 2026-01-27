#include "xbdm_requests.h"

static bool BinarySizeInt32Prefix(uint8_t const* buffer, uint32_t buffer_size,
                                  long& binary_size, uint32_t& bytes_consumed);

GetChecksum::GetChecksum(uint32_t addr, uint32_t len, uint32_t blocksize)
    : RDCPProcessedRequest("getsum") {
  assert((addr & 0x07) == 0);
  assert((len & 0x07) == 0);
  assert((blocksize & 0x07) == 0);

  SetData(" ADDR=");
  AppendHexString(addr);
  AppendData(" LENGTH=");
  AppendHexString(len);
  AppendData(" BLOCKSIZE=");
  AppendHexString(blocksize);

  length = len / blocksize * 4;

  len = length;
  binary_response_size_parser_ = [len](uint8_t const* buffer,
                                       uint32_t buffer_size, long& binary_size,
                                       uint32_t& bytes_consumed) {
    (void)buffer;
    (void)buffer_size;
    binary_size = len;
    bytes_consumed = 0;
    return true;
  };
}

GetExtContext::GetExtContext(uint32_t thread_id)
    : RDCPProcessedRequest("getextcontext") {
  SetData(" thread=");
  AppendHexString(thread_id);

  binary_response_size_parser_ = BinarySizeInt32Prefix;
}

GetFile::GetFile(const std::string& path) : RDCPProcessedRequest("getfile") {
  SetData(" name=\"");
  AppendData(path);
  AppendData("\"");
  binary_response_size_parser_ = BinarySizeInt32Prefix;
}

GetFile::GetFile(const std::string& path, int32_t offset, int32_t size)
    : RDCPProcessedRequest("getfile") {
  SetData(" name=\"");
  AppendData(path);
  AppendData("\" offset=");
  AppendHexString(offset);
  AppendData(" size=");
  AppendHexString(size);
  binary_response_size_parser_ = BinarySizeInt32Prefix;
}

GetGamma::GetGamma() : RDCPProcessedRequest("getgamma") {
  binary_response_size_parser_ = [](uint8_t const* buffer, uint32_t buffer_size,
                                    long& binary_size,
                                    uint32_t& bytes_consumed) {
    (void)buffer;
    (void)buffer_size;
    binary_size = 768;
    bytes_consumed = 0;
    return true;
  };
}

GetMemBinary::GetMemBinary(uint32_t addr, uint32_t length)
    : RDCPProcessedRequest("getmem2"), length(length) {
  SetData(" ADDR=");
  AppendHexString(addr);
  AppendData(" LENGTH=");
  AppendHexString(length);

  binary_response_size_parser_ =
      [length](uint8_t const* buffer, uint32_t buffer_size, long& binary_size,
               uint32_t& bytes_consumed) {
        (void)buffer;
        (void)buffer_size;
        binary_size = length;
        bytes_consumed = 0;
        return true;
      };
}

Screenshot::Screenshot() : RDCPProcessedRequest("screenshot") {
  binary_response_size_parser_ = [this](uint8_t const* buffer,
                                        uint32_t buffer_size, long& binary_size,
                                        uint32_t& bytes_consumed) {
    return ParseSize(buffer, buffer_size, binary_size, bytes_consumed);
  };
}

bool Screenshot::ParseSize(const uint8_t* buffer, uint32_t buffer_size,
                           long& binary_size, uint32_t& bytes_consumed) {
  if (buffer_size < 4) {
    return false;
  }

  // The response is a single line description followed by the actual data.
  auto buffer_end = buffer + buffer_size;
  auto terminator =
      std::search(buffer, buffer_end, RDCPResponse::kTerminator,
                  RDCPResponse::kTerminator + RDCPResponse::kTerminatorLen);
  if (terminator == buffer_end) {
    return false;
  }

  RDCPMapResponse parsed(buffer, terminator);

  pitch = parsed.GetUInt32("pitch");
  width = parsed.GetUInt32("width");
  height = parsed.GetUInt32("height");
  format = parsed.GetUInt32("format");
  binary_size = parsed.GetUInt32("framebuffersize");
  bytes_consumed = terminator - buffer + RDCPResponse::kTerminatorLen;
  return true;
}

static bool BinarySizeInt32Prefix(uint8_t const* buffer, uint32_t buffer_size,
                                  long& binary_size, uint32_t& bytes_consumed) {
  if (buffer_size < 4) {
    return false;
  }

  binary_size = *reinterpret_cast<uint32_t const*>(buffer);
  bytes_consumed = 4;
  return true;
}
