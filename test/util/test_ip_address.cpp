#include <boost/test/unit_test.hpp>

#include "net/ip_address.h"

BOOST_AUTO_TEST_SUITE(ip_address_suite)

BOOST_AUTO_TEST_CASE(IsIPv4Address_ValidIPs) {
  BOOST_TEST(IPAddress::IsIPv4Address("192.168.1.1"));
  BOOST_TEST(IPAddress::IsIPv4Address("10.0.0.24"));
  BOOST_TEST(IPAddress::IsIPv4Address("127.0.0.1"));
  BOOST_TEST(IPAddress::IsIPv4Address("0.0.0.0"));
  BOOST_TEST(IPAddress::IsIPv4Address("255.255.255.255"));
  BOOST_TEST(IPAddress::IsIPv4Address("8.8.8.8"));
  BOOST_TEST(IPAddress::IsIPv4Address("localhost"));
  BOOST_TEST(IPAddress::IsIPv4Address("LocalHost"));
  BOOST_TEST(IPAddress::IsIPv4Address("LOCALHOST"));
}

BOOST_AUTO_TEST_CASE(IsIPv4Address_ValidIPWithPort) {
  BOOST_TEST(IPAddress::IsIPv4Address("192.168.1.1:731"));
  BOOST_TEST(IPAddress::IsIPv4Address("10.0.0.24:1999"));
  BOOST_TEST(IPAddress::IsIPv4Address("127.0.0.1:80"));
  BOOST_TEST(IPAddress::IsIPv4Address("0.0.0.0:0"));
  BOOST_TEST(IPAddress::IsIPv4Address("255.255.255.255:65535"));
  BOOST_TEST(IPAddress::IsIPv4Address("localhost:731"));
  BOOST_TEST(IPAddress::IsIPv4Address("localhost:1999"));
}

BOOST_AUTO_TEST_CASE(IsIPv4Address_ValidPortOnly) {
  BOOST_TEST(IPAddress::IsIPv4Address(":731"));
  BOOST_TEST(IPAddress::IsIPv4Address(":1999"));
  BOOST_TEST(IPAddress::IsIPv4Address(":0"));
  BOOST_TEST(IPAddress::IsIPv4Address(":65535"));
}

BOOST_AUTO_TEST_CASE(IsIPv4Address_CommandsAndWords) {
  BOOST_TEST(!IPAddress::IsIPv4Address("reboot"));
  BOOST_TEST(!IPAddress::IsIPv4Address("break"));
  BOOST_TEST(!IPAddress::IsIPv4Address("gdb"));
  BOOST_TEST(!IPAddress::IsIPv4Address("memwalk"));
  BOOST_TEST(!IPAddress::IsIPv4Address("trace"));
  BOOST_TEST(!IPAddress::IsIPv4Address("help"));
  BOOST_TEST(!IPAddress::IsIPv4Address("threads"));
}

BOOST_AUTO_TEST_CASE(IsIPv4Address_InvalidFormats) {
  BOOST_TEST(!IPAddress::IsIPv4Address(""));
  BOOST_TEST(!IPAddress::IsIPv4Address("192.168.1.999"));
  BOOST_TEST(!IPAddress::IsIPv4Address("192.168.1.1.1"));
  BOOST_TEST(!IPAddress::IsIPv4Address("192.168.1"));
  BOOST_TEST(!IPAddress::IsIPv4Address("192.168.1.1:abc"));
  BOOST_TEST(!IPAddress::IsIPv4Address("192.168.1.1:99999"));
  BOOST_TEST(!IPAddress::IsIPv4Address("192.168.1.1:"));
  BOOST_TEST(!IPAddress::IsIPv4Address(":"));
  BOOST_TEST(!IPAddress::IsIPv4Address("::"));
  BOOST_TEST(!IPAddress::IsIPv4Address(":abc"));
  BOOST_TEST(!IPAddress::IsIPv4Address(":99999"));
  BOOST_TEST(!IPAddress::IsIPv4Address("e:\\games\\default.xbe"));
  BOOST_TEST(!IPAddress::IsIPv4Address("x.y.z.w"));
}

BOOST_AUTO_TEST_CASE(IPAddress_LocalhostConstruction) {
  IPAddress addr("localhost", 731);
  BOOST_TEST(addr.Port() == 731);
  BOOST_TEST(addr.IP().s_addr == htonl(INADDR_LOOPBACK));

  IPAddress addr_port("localhost:1999", 731);
  BOOST_TEST(addr_port.Port() == 1999);
  BOOST_TEST(addr_port.IP().s_addr == htonl(INADDR_LOOPBACK));
}

BOOST_AUTO_TEST_SUITE_END()
