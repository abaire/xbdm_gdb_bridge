#ifndef XBDM_GDB_BRIDGE_SRC_NET_IPTRANSPORT_H_
#define XBDM_GDB_BRIDGE_SRC_NET_IPTRANSPORT_H_

#include <cctype>
#include <list>
#include <string>
#include <sys/socket.h>

class IPTransport {
private:
  std::string name;
  int socket;
  uint32_t address;
  uint16_t port;

  //    std::list<std::filebuf
  //  self.addr: Optional[Tuple[str, int]] = None
  //  self._read_buffer: bytearray = bytearray()
  //  self._write_buffer_lock: threading.RLock = threading.RLock()
  //  self._write_buffer: bytearray = bytearray()
  //  self._on_bytes_read: Optional[Callable[[IPTransport], None]] =
  //  process_callback
};

#endif // XBDM_GDB_BRIDGE_SRC_NET_IPTRANSPORT_H_
