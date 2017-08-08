#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include "cervuspp.h"

class ServerContext : public BaseServerContext {
    private:
        int count;

    public:
        virtual void on_context_init(Resource ctx) {
            cervus_log("Initializing ServerContext");
        }

        virtual void before_request(BasicRequestInfo *info) {
            //printf("[%d] %s %s %s\n", next_request_id, info -> remote_addr, info -> method, info -> uri);
            if(strncmp(info -> uri, "/denied", 7) == 0) {
                Resource resp = ice_glue_create_response();
                ice_glue_response_set_status(resp, 403);

                info -> response = resp;
            } else {
                char count_str[128];
                snprintf(count_str, 127, "%d", count++);

                ice_glue_custom_properties_set(info -> custom_properties, "request_id", count_str);
            }
        }
};

void _do_context_init(BaseServerContext **indirect, Resource ctx) {
    *indirect = (ServerContext *) malloc(sizeof(ServerContext));
    new(*indirect) ServerContext();
    //*indirect = new ServerContext();
    (*indirect) -> on_context_init(ctx);
}
