#include "gdb_registers.h"

#include <arpa/inet.h>

#include "configure.h"
#include "rdcp/types/thread_context.h"

static void AppendRegister(std::string& output,
                           const std::string& register_name,
                           const std::optional<int32_t>& value) {
  char buffer[32] = {0};
  if (!value.has_value()) {
    output += "xxxxxxxx";
    return;
  }

  snprintf(buffer, 31, "%08x", htonl(*value));
  output += buffer;
}

static void Append10ByteRegister(std::string& output,
                                 const std::string& register_name,
                                 const std::optional<uint64_t>& value) {
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

std::optional<uint64_t> GetRegister(
    uint32_t gdb_index, const std::optional<ThreadContext>& context,
    const std::optional<ThreadFloatContext>& float_context) {
  if (!context) {
    return {};
  }

  switch (gdb_index) {
    case 0:
      return context->eax;
    case 1:
      return context->ecx;
    case 2:
      return context->edx;
    case 3:
      return context->ebx;
    case 4:
      return context->esp;
    case 5:
      return context->ebp;
    case 6:
      return context->esi;
    case 7:
      return context->edi;
    case 8:
      return context->eip;
    case 9:
      return context->eflags;

    case FLOAT_REGISTER_OFFSET:
      return float_context->st0;
    case FLOAT_REGISTER_OFFSET + 1:
      return float_context->st1;
    case FLOAT_REGISTER_OFFSET + 2:
      return float_context->st2;
    case FLOAT_REGISTER_OFFSET + 3:
      return float_context->st3;
    case FLOAT_REGISTER_OFFSET + 4:
      return float_context->st4;
    case FLOAT_REGISTER_OFFSET + 5:
      return float_context->st5;
    case FLOAT_REGISTER_OFFSET + 6:
      return float_context->st6;
    case FLOAT_REGISTER_OFFSET + 7:
      return float_context->st7;

    default:
      break;
  }

  return {};
}

bool SetRegister(uint32_t gdb_index, uint32_t value,
                 std::optional<ThreadContext>& context) {
  if (!context) {
    return false;
  }

  switch (gdb_index) {
    case 0:
      context->eax = value;
      break;
    case 1:
      context->ecx = value;
      break;
    case 2:
      context->edx = value;
      break;
    case 3:
      context->ebx = value;
      break;
    case 4:
      context->esp = value;
      break;
    case 5:
      context->ebp = value;
      break;
    case 6:
      context->esi = value;
      break;
    case 7:
      context->edi = value;
      break;
    case 8:
      context->eip = value;
      break;
    case 9:
      context->eflags = value;
      break;
    default:
      return false;
  }

  return true;
}

bool SetRegister(uint32_t gdb_index, uint64_t value,
                 std::optional<ThreadFloatContext>& float_context) {
  if (!float_context) {
    return false;
  }

  switch (gdb_index) {
    case FLOAT_REGISTER_OFFSET:
      float_context->st0 = static_cast<int64_t>(value);
      break;
    case FLOAT_REGISTER_OFFSET + 1:
      float_context->st1 = static_cast<int64_t>(value);
      break;
    case FLOAT_REGISTER_OFFSET + 2:
      float_context->st2 = static_cast<int64_t>(value);
      break;
    case FLOAT_REGISTER_OFFSET + 3:
      float_context->st3 = static_cast<int64_t>(value);
      break;
    case FLOAT_REGISTER_OFFSET + 4:
      float_context->st4 = static_cast<int64_t>(value);
      break;
    case FLOAT_REGISTER_OFFSET + 5:
      float_context->st5 = static_cast<int64_t>(value);
      break;
    case FLOAT_REGISTER_OFFSET + 6:
      float_context->st6 = static_cast<int64_t>(value);
      break;
    case FLOAT_REGISTER_OFFSET + 7:
      float_context->st7 = static_cast<int64_t>(value);
      break;
    default:
      return false;
  }

  return true;
}

std::string SerializeRegisters(
    const std::optional<ThreadContext>& context,
    const std::optional<ThreadFloatContext>& float_context) {
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
