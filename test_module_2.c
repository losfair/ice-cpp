#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cervus.h"
#include "imports.h"

struct ModuleState {
    int req_id;
};

void before_request(Resource hook_ctx) {
    //cervus_log(7, "before_request called");
    struct ModuleState *state = get_module_mem();
    state -> req_id++;
    struct BasicRequestInfo *bri = (struct BasicRequestInfo *) downcast_hook_context(hook_ctx, "basic_request_info");
    bri -> response = ice_glue_create_response();
    //ice_glue_response_set_status(bri -> response, 403);
    //printf("Request id: %d\n", state -> req_id);
}

void after_response(Resource hook_ctx) {
    //cervus_log(7, "after_response called");
    Resource resp = downcast_hook_context(hook_ctx, "glue_response");
}

void interop_ping(Resource hook_ctx) {
    Resource interop_ctx = downcast_hook_context(hook_ctx, "interop_context");
    const char *v = ice_glue_interop_get_tx_field(interop_ctx, "value");
    if(!v || strcmp(v, "Ping") != 0) {
        return;
    }
    ice_glue_interop_set_rx_field(interop_ctx, "value", "Pong");
}

void module_init() {
    void * addr = reset_module_mem(sizeof(struct ModuleState));
    printf("%p\n", addr);
    //add_hook("before_request", before_request);
    add_hook("after_response", after_response);
    add_hook("interop_ping", interop_ping);
    cervus_log(7, "Test module initialized");
}
