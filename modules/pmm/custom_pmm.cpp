#include "../kernel/kernel_api.h"
#include "../kernel/module_api.h"


static const KernelAPI* g_kapi = nullptr;

static  void* custompmm_init(const KernelAPI* api);

__attribute__((section(".modules")))
static ModuleHeader mod_header ={
  .magic = MAGICNUM,
  .name = "PMM_TEST_Dummy",
  .type = MT_PMM,
  .module_init = custompmm_init 
};

static uintptr_t dummy_alloc(int n)
{
  g_kapi->log("[CUSTOM_PMM] Module dummy_alloc called for %d page(s)!\n", n);
  return g_kapi->alloc_page(n);
}

static void dumm_free(uint64_t addr)
{
  g_kapi->log("[CUSTOM_PMM] Module dummy_free called for address %x!\n", addr);
  g_kapi->free_page(addr);
}

static void* custompmm_init(const KernelAPI* api)
{
  g_kapi = api;
  g_kapi->log("[MODULE] Custom PMM Dummy Module Loaded Successfully!\n");

  static PMM_API pmmapi;
  pmmapi.alloc_page = dummy_alloc;
  pmmapi.free_page = dumm_free;
  return &pmmapi;
}

