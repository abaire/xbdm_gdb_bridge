#ifdef POPULATE_MAP
#define DECLARE(base_name, string_name, ordinal) \
  { "_" string_name, XBDM_##base_name }

#define END ,
#define LAST_END
#else
#define DECLARE(base_name, string_name, ordinal) \
  static constexpr uint32_t XBDM_##base_name = (ordinal)
#define END ;
#define LAST_END ;
#endif

// clang-format: off
DECLARE(DmAllocatePoolWithTag, "DmAllocatePoolWithTag@8", 2)
END DECLARE(DmFreePool, "DmFreePool@4", 9) END
    DECLARE(DmRegisterCommandProcessor, "DmRegisterCommandProcessor@8", 30) END
    DECLARE(DmResumeThread, "DmResumeThread@4", 35) LAST_END
// clang-format: on

#undef END
#undef LAST_END
#undef DECLARE
