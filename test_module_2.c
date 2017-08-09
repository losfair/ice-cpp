#include <stdio.h>
#include <stdlib.h>
#include "cervus.h"

void before_request(Memory mem, struct BasicRequestInfo *info) {
    //printf("%p %p\n", mem, info);
    Resource cp = info -> custom_properties;
    ice_glue_custom_properties_set(cp, "request_uri", info -> uri);
}

void after_response(Memory mem, Resource resp, Resource cp) {
    const char *uri = ice_glue_custom_properties_get(cp, "request_uri");
    printf("Ended: %s\n", uri);
}

void cervus_module_init(struct ModuleInitConfig *cfg) {
    cfg -> ok = 1;
    cfg -> before_request_hook = before_request;
    cfg -> after_response_hook = after_response;
    cervus_log("Test module 2 loaded");
    Resource m = malloc(1048576);
    free(m);
}
