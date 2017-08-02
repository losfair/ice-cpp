#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <deque>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <unistd.h>
#include <uv.h>
#include <libgccjit.h>
#include "imports.h"

namespace ice {

class Request;
static void dispatch_task(uv_async_t *task_async);

typedef std::function<void(Request)> DispatchTarget;
typedef std::function<void()> Task;

class Response {
    public:
        Resource call_info;
        Resource handle;

        Response(Resource _call_info) {
            call_info = _call_info;
            handle = ice_glue_create_response();
        }

        Response& set_body(const u8 *body, u32 len) {
            ice_glue_response_set_body(handle, body, len);
            return *this;
        }
        
        Response& set_body(const char *body) {
            return set_body((const u8 *) body, strlen(body));
        }

        Response& set_body(const std::string& body) {
            return set_body((const u8 *) body.c_str(), body.size());
        }

        Response& set_status(u16 status) {
            ice_glue_response_set_status(handle, status);
            return *this;
        }

        void send() {
            ice_core_fire_callback(call_info, handle);
        }
};

class Request {
    public:
        Resource call_info;
        Resource handle;

        Request(Resource _call_info) {
            call_info = _call_info;
            handle = ice_core_borrow_request_from_call_info(call_info);
        }

        Response create_response() {
            return Response(call_info);
        }
};

class Server {
    public:
        Resource handle;
        uv_loop_t *ev_loop;
        uv_async_t task_async;
        std::unordered_map<int, DispatchTarget> dispatch_table;
        std::mutex pending_mutex;
        std::deque<Task> pending;
        std::string async_endpoint_cb_signature;
        AsyncEndpointHandler target_async_endpoint_cb;

        Server(uv_loop_t *loop) {
            handle = ice_create_server();
            if(loop) {
                ev_loop = loop;
            } else {
                ev_loop = uv_default_loop();
            }
            uv_async_init(ev_loop, &task_async, ice::dispatch_task);
            task_async.data = (void *) this;

            std::stringstream ss;
            ss << "async_endpoint_cb_" << this;
            ss >> async_endpoint_cb_signature;

            target_async_endpoint_cb = NULL;
        }

        ~Server() {}

        void listen(const char *addr) {
            ice_server_listen(handle, addr);
        }

        void add_endpoint(const char *path, DispatchTarget handler, std::vector<std::string> flags) {
            Resource ep = ice_server_router_add_endpoint(handle, path);
            for(auto& f : flags) {
                ice_core_endpoint_set_flag(ep, f.c_str(), true);
            }

            int id = ice_core_endpoint_get_id(ep);

            dispatch_table[id] = handler;
        }

        void add_endpoint(const char *path, DispatchTarget handler) {
            add_endpoint(path, handler, std::vector<std::string>());
        }

        void disable_request_logging() {
            ice_server_disable_request_logging(handle);
        }

        static void dispatch_async_endpoint_cb(Server *server, int ep_id, Resource call_info) {
            server -> async_endpoint_cb(ep_id, call_info);
        }

        AsyncEndpointHandler leaky_generate_async_endpoint_cb() {
            gcc_jit_context *ctx = gcc_jit_context_acquire();
            if(!ctx) throw std::runtime_error("Unable to get JIT context");

            gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
            gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);

            gcc_jit_param *ep_id_param = gcc_jit_context_new_param(ctx, NULL, int_type, "ep_id");
            gcc_jit_param *call_info_param = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "call_info");

            gcc_jit_param *params[] = {
                ep_id_param,
                call_info_param
            };

            gcc_jit_function *func = gcc_jit_context_new_function(
                ctx,
                NULL,
                GCC_JIT_FUNCTION_EXPORTED,
                void_type,
                this -> async_endpoint_cb_signature.c_str(),
                2,
                params,
                0
            );

            gcc_jit_rvalue *args[3];
            args[0] = gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *) this);
            args[1] = gcc_jit_param_as_rvalue(ep_id_param);
            args[2] = gcc_jit_param_as_rvalue(call_info_param);

            gcc_jit_type *target_param_types[] = {
                void_ptr_type,
                int_type,
                void_ptr_type
            };
            
            gcc_jit_rvalue *target_dispatcher = gcc_jit_context_new_rvalue_from_ptr(
                ctx,
                gcc_jit_context_new_function_ptr_type(
                    ctx,
                    NULL,
                    void_type,
                    3,
                    target_param_types,
                    0
                ),
                (void *) Server::dispatch_async_endpoint_cb
            );

            gcc_jit_block *function_block = gcc_jit_function_new_block(func, NULL);
            gcc_jit_block_add_eval(
                function_block,
                NULL,
                gcc_jit_context_new_call_through_ptr(
                    ctx,
                    NULL,
                    target_dispatcher,
                    3,
                    args
                )
            );
            gcc_jit_block_end_with_void_return(function_block, NULL);

            gcc_jit_result *result = gcc_jit_context_compile(ctx); // This leaks
            if(!result) throw std::runtime_error("Unable to do JIT compile");

            AsyncEndpointHandler generated_cb = (AsyncEndpointHandler) gcc_jit_result_get_code(result, this -> async_endpoint_cb_signature.c_str());
            gcc_jit_context_release(ctx);

            return generated_cb;
        }

        void async_endpoint_cb(int ep_id, Resource call_info) {
            Request req(call_info);

            auto target = dispatch_table[ep_id];
            if(!target) {
                //std::cerr << "Error: Calling an invalid endpoint: " << ep_id << std::endl;
                req.create_response().set_status(404).set_body("Invalid endpoint").send();
                return;
            }

            pending_mutex.lock();
            pending.push_back([=]() {
                target(req);
            });
            pending_mutex.unlock();
            uv_async_send(&task_async);
        }

        void run(const char *addr) {
            target_async_endpoint_cb = leaky_generate_async_endpoint_cb();
            ice_server_set_async_endpoint_cb(handle, target_async_endpoint_cb);
            
            listen(addr);
            uv_run(ev_loop, UV_RUN_DEFAULT);
        }

        void dispatch_task() {
            pending_mutex.lock();
            std::deque<Task> current_tasks = pending;
            pending.clear();
            pending_mutex.unlock();

            for(auto& t : current_tasks) {
                t();
            }
        }
};

static void dispatch_task(uv_async_t *task_async) {
    Server *server = (Server *) (task_async -> data);
    server -> dispatch_task();
}

} // namespace ice
