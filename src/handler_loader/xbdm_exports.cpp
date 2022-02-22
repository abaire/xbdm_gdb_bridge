#include "xbdm_exports.h"

std::map<std::string, uint32_t> XBDM_Exports = {

#define POPULATE_MAP
#include "xbdm_exports.def.h"

};
