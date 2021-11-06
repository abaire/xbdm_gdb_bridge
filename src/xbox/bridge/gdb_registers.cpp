#include "gdb_registers.h"

#include <arpa/inet.h>

#include "configure.h"
#include "rdcp/types/thread_context.h"

static void AppendRegister(std::string &output,
                           const std::string &register_name,
                           const std::optional<int32_t> &value) {
  char buffer[32] = {0};
  if (!value.has_value()) {
    output += "xxxxxxxx";
    return;
  }

  snprintf(buffer, 31, "%08x", htonl(*value));
  output += buffer;
}

static void Append10ByteRegister(std::string &output,
                                 const std::string &register_name,
                                 const std::optional<uint64_t> &value) {
  char buffer[64] = {0};
  if (!value.has_value()) {
    output += "xxxxxxxxxxxxxxxxxxxx";
    return;
  }

#if defined(HAVE_HTONLL)
  snprintf(buffer, 63, "%020llx", htonll(*value));
#elif defined(HAVE_BE64TOH)
  snprintf(buffer, 63, "%020lx", htole64(*value));
#else
#error "No 64-bit byte swapping function for this platform."
#endif
  // Truncate if the value has digits beyond the first 10 bytes.
  buffer[20] = 0;
  output += buffer;
}

std::string SerializeRegisters(
    std::optional<ThreadContext> context,
    std::optional<ThreadFloatContext> float_context) {
  std::string ret;

  std::optional<int32_t> unsupported;
  if (context.has_value()) {
    AppendRegister(ret, "Eax", context->eax);
    AppendRegister(ret, "Ecx", context->ecx);
    AppendRegister(ret, "Edx", context->edx);
    AppendRegister(ret, "Ebx", context->ebx);
    AppendRegister(ret, "Esp", context->esp);
    AppendRegister(ret, "Ebp", context->ebp);
    AppendRegister(ret, "Esi", context->esi);
    AppendRegister(ret, "Edi", context->edi);
    AppendRegister(ret, "Eip", context->eip);
    AppendRegister(ret, "EFlags", context->eflags);
  } else {
    AppendRegister(ret, "Eax", unsupported);
    AppendRegister(ret, "Ecx", unsupported);
    AppendRegister(ret, "Edx", unsupported);
    AppendRegister(ret, "Ebx", unsupported);
    AppendRegister(ret, "Esp", unsupported);
    AppendRegister(ret, "Ebp", unsupported);
    AppendRegister(ret, "Esi", unsupported);
    AppendRegister(ret, "Edi", unsupported);
    AppendRegister(ret, "Eip", unsupported);
    AppendRegister(ret, "EFlags", unsupported);
  }

  AppendRegister(ret, "cs", unsupported);
  AppendRegister(ret, "ss", unsupported);
  AppendRegister(ret, "ds", unsupported);
  AppendRegister(ret, "es", unsupported);
  AppendRegister(ret, "fs", unsupported);
  AppendRegister(ret, "gs", unsupported);
  AppendRegister(ret, "ss_base", unsupported);
  AppendRegister(ret, "ds_base", unsupported);
  AppendRegister(ret, "es_base", unsupported);
  AppendRegister(ret, "fs_base", unsupported);
  AppendRegister(ret, "gs_base", unsupported);
  AppendRegister(ret, "k_gs_base", unsupported);
  AppendRegister(ret, "cr0", unsupported);
  AppendRegister(ret, "cr2", unsupported);
  AppendRegister(ret, "cr3", unsupported);
  AppendRegister(ret, "cr4", unsupported);
  AppendRegister(ret, "cr8", unsupported);
  AppendRegister(ret, "efer", unsupported);

  if (float_context.has_value()) {
    Append10ByteRegister(ret, "ST0", float_context->st0);
    Append10ByteRegister(ret, "ST1", float_context->st1);
    Append10ByteRegister(ret, "ST2", float_context->st2);
    Append10ByteRegister(ret, "ST3", float_context->st3);
    Append10ByteRegister(ret, "ST4", float_context->st4);
    Append10ByteRegister(ret, "ST5", float_context->st5);
    Append10ByteRegister(ret, "ST6", float_context->st6);
    Append10ByteRegister(ret, "ST7", float_context->st7);
  } else {
    Append10ByteRegister(ret, "ST0", unsupported);
    Append10ByteRegister(ret, "ST1", unsupported);
    Append10ByteRegister(ret, "ST2", unsupported);
    Append10ByteRegister(ret, "ST3", unsupported);
    Append10ByteRegister(ret, "ST4", unsupported);
    Append10ByteRegister(ret, "ST5", unsupported);
    Append10ByteRegister(ret, "ST6", unsupported);
    Append10ByteRegister(ret, "ST7", unsupported);
  }
  AppendRegister(ret, "fctrl", unsupported);
  AppendRegister(ret, "fstat", unsupported);
  AppendRegister(ret, "ftag", unsupported);
  AppendRegister(ret, "fiseg", unsupported);
  AppendRegister(ret, "fioff", unsupported);
  AppendRegister(ret, "foseg", unsupported);
  AppendRegister(ret, "fooff", unsupported);
  AppendRegister(ret, "fop", unsupported);

  return std::move(ret);
}
