#ifndef XBDM_GDB_BRIDGE_SRC_NET_IP_TRANSPORT_H_
#define XBDM_GDB_BRIDGE_SRC_NET_IP_TRANSPORT_H_

#include <cctype>
#include <list>
#include <string>
#include <sys/socket.h>

class IPTransport {
private:
  std::string name_;
  int socket_;
  uint32_t address_;
  uint16_t port_;

  //    std::list<std::filebuf
  //  self.addr: Optional[Tuple[str, int]] = None
  //  self._read_buffer: bytearray = bytearray()
  //  self._write_buffer_lock: threading.RLock = threading.RLock()
  //  self._write_buffer: bytearray = bytearray()
  //  self._on_bytes_read: Optional[Callable[[IPTransport], None]] =
  //  process_callback
};

#endif // XBDM_GDB_BRIDGE_SRC_NET_IP_TRANSPORT_H_
