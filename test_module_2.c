#include <stdio.h>
#include <stdlib.h>
#include "cervus.h"

void before_request(Memory mem, struct BasicRequestInfo *info) {
    //printf("%p %p\n", mem, info);
}

void cervus_module_init(struct ModuleInitConfig *cfg) {
    cfg -> ok = 1;
    cfg -> before_request_hook = before_request;
    cervus_log("Test module 2 loaded");
    Resource m = malloc(1048576);
    free(m);
}
