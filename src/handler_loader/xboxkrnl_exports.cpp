#include "xboxkrnl_exports.h"

std::map<std::string, uint32_t> XBOXKRNL_Exports = {

#define POPULATE_MAP
#include "xboxkrnl_exports.def.h"

};
