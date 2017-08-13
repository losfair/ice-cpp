#ifndef _CERVUS_H_
#define _CERVUS_H_

#include "imports.h"

struct BasicRequestInfo {
    const char *uri;
    const char *remote_addr;
    const char *method;
    Resource response;
    Resource custom_properties;
};

typedef void (*ContextInitHookFn)(Memory mem, Resource ctx);
typedef void (*ContextDestroyHookFn)(Memory mem, Resource ctx);
typedef void (*BeforeRequestHookFn)(Memory mem, struct BasicRequestInfo *info);
typedef void (*RequestHookFn)(Memory mem, Resource req);
typedef void (*ResponseHookFn)(Memory mem, Resource resp);
typedef void (*AfterResponseHookFn)(Memory mem, Resource resp, Resource custom_properties);

typedef void (*HookFn)(Resource hook_context);

struct ModuleInitConfig {
    char ok;
    unsigned int server_context_mem_size;
    ContextInitHookFn context_init_hook;
    ContextDestroyHookFn context_destroy_hook;
    BeforeRequestHookFn before_request_hook;
    RequestHookFn request_hook;
    ResponseHookFn response_hook;
    AfterResponseHookFn after_response_hook;
};

#ifdef __cplusplus
extern "C" {
#endif

void cervus_log(int level, const char *msg);
void add_hook(const char *name, HookFn cb);
Resource downcast_hook_context(Resource hook_context, const char *target_type);
void * reset_module_mem(unsigned int size);
Resource get_module_mem();
#ifdef __cplusplus
}
#endif

#endif
