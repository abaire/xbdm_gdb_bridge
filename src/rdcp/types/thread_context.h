#ifndef XBDM_GDB_BRIDGE_THREAD_CONTEXT_H
#define XBDM_GDB_BRIDGE_THREAD_CONTEXT_H

#include <cinttypes>
#include <cstring>
#include <optional>
#include <ostream>

#include "rdcp/rdcp_response_processors.h"
#include "util/optional.h"

struct ThreadContext {
  std::optional<int32_t> ebp;
  std::optional<int32_t> esp;
  std::optional<int32_t> eip;
  std::optional<int32_t> eflags;
  std::optional<int32_t> eax;
  std::optional<int32_t> ebx;
  std::optional<int32_t> ecx;
  std::optional<int32_t> edx;
  std::optional<int32_t> edi;
  std::optional<int32_t> esi;
  std::optional<int32_t> cr0_npx_state;

  void Parse(const RDCPMapResponse& parsed) {
    ebp = parsed.GetOptionalDWORD("Ebp");
    esp = parsed.GetOptionalDWORD("Esp");
    eip = parsed.GetOptionalDWORD("Eip");
    eflags = parsed.GetOptionalDWORD("EFlags");
    eax = parsed.GetOptionalDWORD("Eax");
    ebx = parsed.GetOptionalDWORD("Ebx");
    ecx = parsed.GetOptionalDWORD("Ecx");
    edx = parsed.GetOptionalDWORD("Edx");
    edi = parsed.GetOptionalDWORD("Edi");
    esi = parsed.GetOptionalDWORD("Esi");
    cr0_npx_state = parsed.GetOptionalDWORD("Cr0NpxState");
  }

  [[nodiscard]] std::string Serialize() const {
    std::string ret;

    char buf[64] = {0};
    if (ebp.has_value()) {
      snprintf(buf, 63, " Ebp=0x%x", ebp.value());
      ret += buf;
    }
    if (esp.has_value()) {
      snprintf(buf, 63, " Esp=0x%x", esp.value());
      ret += buf;
    }
    if (eip.has_value()) {
      snprintf(buf, 63, " Eip=0x%x", eip.value());
      ret += buf;
    }
    if (eflags.has_value()) {
      snprintf(buf, 63, " EFlags=0x%x", eflags.value());
      ret += buf;
    }
    if (eax.has_value()) {
      snprintf(buf, 63, " Eax=0x%x", eax.value());
      ret += buf;
    }
    if (ebx.has_value()) {
      snprintf(buf, 63, " Ebx=0x%x", ebx.value());
      ret += buf;
    }
    if (ecx.has_value()) {
      snprintf(buf, 63, " Ecx=0x%x", ecx.value());
      ret += buf;
    }
    if (edx.has_value()) {
      snprintf(buf, 63, " Edx=0x%x", edx.value());
      ret += buf;
    }
    if (edi.has_value()) {
      snprintf(buf, 63, " Edi=0x%x", edi.value());
      ret += buf;
    }
    if (esi.has_value()) {
      snprintf(buf, 63, " Esi=0x%x", esi.value());
      ret += buf;
    }
    if (cr0_npx_state.has_value()) {
      snprintf(buf, 63, " Cr0NpxState=0x%x", cr0_npx_state.value());
      ret += buf;
    }

    return std::move(ret);
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const ThreadContext& context) {
    os << std::hex << "ebp: " << context.ebp << " esp: " << context.esp
       << " eip: " << context.eip << " eflags: " << context.eflags
       << " eax: " << context.eax << " ebx: " << context.ebx
       << " ecx: " << context.ecx << " edx: " << context.edx
       << " edi: " << context.edi << " esi: " << context.esi
       << " cr0_npx_state: " << context.cr0_npx_state;
    return os;
  }
};

struct ThreadFloatContext {
  int32_t control{0};
  int32_t status{0};
  int32_t tag{0};
  int32_t error_offset{0};
  int32_t error_selector{0};
  int32_t data_offset{0};
  int32_t data_selector{0};
  int64_t st0{0};
  int64_t st1{0};
  int64_t st2{0};
  int64_t st3{0};
  int64_t st4{0};
  int64_t st5{0};
  int64_t st6{0};
  int64_t st7{0};
  int32_t cr0_npx_state{0};

  void Parse(const std::vector<char>& buffer) {
    auto data = reinterpret_cast<const uint8_t*>(buffer.data());
    auto values = reinterpret_cast<const int32_t*>(data);
    control = *values++;
    status = *values++;
    tag = *values++;
    error_offset = *values++;
    error_selector = *values++;
    data_offset = *values++;
    data_selector = *values++;

    data = reinterpret_cast<const uint8_t*>(values);
    memcpy(&st0, data, 10);
    data += 10;
    memcpy(&st1, data, 10);
    data += 10;
    memcpy(&st2, data, 10);
    data += 10;
    memcpy(&st3, data, 10);
    data += 10;
    memcpy(&st4, data, 10);
    data += 10;
    memcpy(&st5, data, 10);
    data += 10;
    memcpy(&st6, data, 10);
    data += 10;
    memcpy(&st7, data, 10);
    data += 10;

    cr0_npx_state = *reinterpret_cast<const int32_t*>(data);
  }

  [[nodiscard]] std::vector<uint8_t> Serialize() const {
    std::vector<uint8_t> ret(8 * 4 + 80);
    auto data = ret.data();
    auto values = reinterpret_cast<int32_t*>(data);
    *values++ = control;
    *values++ = status;
    *values++ = tag;
    *values++ = error_offset;
    *values++ = error_selector;
    *values++ = data_offset;
    *values++ = data_selector;

    data = reinterpret_cast<uint8_t*>(values);
    memcpy(data, &st0, 10);
    data += 10;
    memcpy(data, &st1, 10);
    data += 10;
    memcpy(data, &st2, 10);
    data += 10;
    memcpy(data, &st3, 10);
    data += 10;
    memcpy(data, &st4, 10);
    data += 10;
    memcpy(data, &st5, 10);
    data += 10;
    memcpy(data, &st6, 10);
    data += 10;
    memcpy(data, &st7, 10);
    data += 10;

    values = reinterpret_cast<int32_t*>(data);
    *values = cr0_npx_state;

    return std::move(ret);
  }

  friend std::ostream& operator<<(std::ostream& os,
                                  const ThreadFloatContext& context) {
    os << std::hex << "control: " << context.control
       << " status: " << context.status << " tag: " << context.tag
       << " error_offset: " << context.error_offset
       << " error_selector: " << context.error_selector
       << " data_offset: " << context.data_offset
       << " data_selector: " << context.data_selector << " st0: " << context.st0
       << " st1: " << context.st1 << " st2: " << context.st2
       << " st3: " << context.st3 << " st4: " << context.st4
       << " st5: " << context.st5 << " st6: " << context.st6
       << " st7: " << context.st7
       << " cr0_npx_state: " << context.cr0_npx_state;
    return os;
  }
};

#endif  // XBDM_GDB_BRIDGE_THREAD_CONTEXT_H
