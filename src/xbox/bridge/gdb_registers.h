#ifndef XBDM_GDB_BRIDGE_GDB_REGISTERS_H
#define XBDM_GDB_BRIDGE_GDB_REGISTERS_H

#include <cstdint>
#include <optional>
#include <string>

class ThreadContext;
class ThreadFloatContext;

// GDB index of the first XBDM float context register
#define FLOAT_REGISTER_OFFSET 19

constexpr std::string_view kTargetXML =
    R"(<?xml version="1.0"?><!DOCTYPE target SYSTEM "gdb-target.dtd"><target>)"
    "<architecture>i386:intel</architecture>"
    R"(<feature name="i386.xbdm"/>)"
    R"(<flags id="i386_eflags" size="4">)"
    R"(<field name="CF" start="0" end="0"/>)"
    R"(<field name="PF" start="2" end="2"/>)"
    R"(<field name="AF" start="4" end="4"/>)"
    R"(<field name="ZF" start="6" end="6"/>)"
    R"(<field name="SF" start="7" end="7"/>)"
    R"(<field name="TF" start="8" end="8"/>)"
    R"(<field name="IF" start="9" end="9"/>)"
    R"(<field name="DF" start="10" end="10"/>)"
    R"(<field name="OF" start="11" end="11"/>)"
    R"(<field name="IOPL" start="12" end="13"/>)"
    R"(<field name="NT" start="14" end="14"/>)"
    R"(<field name="RF" start="16" end="16"/>)"
    R"(<field name="VM" start="17" end="17"/>)"
    R"(<field name="AC" start="18" end="18"/>)"
    R"(<field name="VIF" start="19" end="19"/>)"
    R"(<field name="VIP" start="20" end="20"/>)"
    R"(<field name="ID" start="21" end="21"/>)"
    "</flags>"

    R"(<flags id="i386_cr0" size="4">)"
    R"(<field name="PG" start="31" end="31"/>)"
    R"(<field name="CD" start="30" end="30"/>)"
    R"(<field name="NW" start="29" end="29"/>)"
    R"(<field name="AM" start="18" end="18"/>)"
    R"(<field name="WP" start="16" end="16"/>)"
    R"(<field name="NE" start="5" end="5"/>)"
    R"(<field name="ET" start="4" end="4"/>)"
    R"(<field name="TS" start="3" end="3"/>)"
    R"(<field name="EM" start="2" end="2"/>)"
    R"(<field name="MP" start="1" end="1"/>)"
    R"(<field name="PE" start="0" end="0"/>)"
    "</flags>"
    R"(<reg name="Eax" bitsize="4" type="int32" regnum="0"/>)"
    R"(<reg name="Ecx" bitsize="4" type="int32"/>)"
    R"(<reg name="Edx" bitsize="4" type="int32"/>)"
    R"(<reg name="Ebx" bitsize="4" type="int32"/>)"
    R"(<reg name="Esp" bitsize="4" type="data_ptr"/>)"
    R"(<reg name="Ebp" bitsize="4" type="data_ptr"/>)"
    R"(<reg name="Esi" bitsize="4" type="int32"/>)"
    R"(<reg name="Edi" bitsize="4" type="int32"/>)"
    R"(<reg name="Eip" bitsize="4" type="code_ptr"/>)"
    R"(<reg name="EFlags" bitsize="4" type="int32"/>)"
    R"(<reg name="cs" bitsize="4" type="int32"/>)"
    R"(<reg name="ss" bitsize="4" type="int32"/>)"
    R"(<reg name="ds" bitsize="4" type="int32"/>)"
    R"(<reg name="es" bitsize="4" type="int32"/>)"
    R"(<reg name="fs" bitsize="4" type="int32"/>)"
    R"(<reg name="gs" bitsize="4" type="int32"/>)"
    R"(<reg name="ss_base" bitsize="4" type="int32"/>)"
    R"(<reg name="ds_base" bitsize="4" type="int32"/>)"
    R"(<reg name="es_base" bitsize="4" type="int32"/>)"
    R"(<reg name="fs_base" bitsize="4" type="int32"/>)"
    R"(<reg name="gs_base" bitsize="4" type="int32"/>)"
    R"(<reg name="k_gs_base" bitsize="4" type="int32"/>)"
    R"(<reg name="cr0" bitsize="4" type="int32"/>)"
    R"(<reg name="cr2" bitsize="4" type="int32"/>)"
    R"(<reg name="cr3" bitsize="4" type="int32"/>)"
    R"(<reg name="cr4" bitsize="4" type="int32"/>)"
    R"(<reg name="cr8" bitsize="4" type="int32"/>)"
    R"(<reg name="efer" bitsize="4" type="int32"/>)"
    R"(<reg name="ST0" bitsize="10" type="i387_ext"/>)"
    R"(<reg name="ST1" bitsize="10" type="i387_ext"/>)"
    R"(<reg name="ST2" bitsize="10" type="i387_ext"/>)"
    R"(<reg name="ST3" bitsize="10" type="i387_ext"/>)"
    R"(<reg name="ST4" bitsize="10" type="i387_ext"/>)"
    R"(<reg name="ST5" bitsize="10" type="i387_ext"/>)"
    R"(<reg name="ST6" bitsize="10" type="i387_ext"/>)"
    R"(<reg name="ST7" bitsize="10" type="i387_ext"/>)"
    R"(<reg name="fctrl" bitsize="4" type="int" group="float"/>)"
    R"(<reg name="fstat" bitsize="4" type="int" group="float"/>)"
    R"(<reg name="ftag" bitsize="4" type="int" group="float"/>)"
    R"(<reg name="fiseg" bitsize="4" type="int" group="float"/>)"
    R"(<reg name="fioff" bitsize="4" type="int" group="float"/>)"
    R"(<reg name="foseg" bitsize="4" type="int" group="float"/>)"
    R"(<reg name="fooff" bitsize="4" type="int" group="float"/>)"
    R"(<reg name="fop" bitsize="4" type="int" group="float"/>)"
    "</target>";

std::string SerializeRegisters(
    const std::optional<ThreadContext>& context,
    const std::optional<ThreadFloatContext>& float_context);

std::optional<uint64_t> GetRegister(
    uint32_t gdb_index, const std::optional<ThreadContext>& context,
    const std::optional<ThreadFloatContext>& float_context);
bool SetRegister(uint32_t gdb_index, uint32_t value,
                 std::optional<ThreadContext>& context);
bool SetRegister(uint32_t gdb_index, uint64_t value,
                 std::optional<ThreadFloatContext>& context);

#endif  // XBDM_GDB_BRIDGE_GDB_REGISTERS_H
