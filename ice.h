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
#include "imports.h"

namespace ice {

class Request;
class Response;
static void dispatch_task(uv_async_t *task_async);

typedef std::function<void(Request)> DispatchTarget;
typedef std::function<Response(Request)> SyncDispatchTarget;

std::unordered_map<std::string, const char *> read_map(const u8 *raw) {
    if(!raw) {
        return std::unordered_map<std::string, const char *>();
    }

    const u8 *p = raw;
    char type_buf[64];

    u16 type_len = * (u16 *) p;
    p += 2;

    memcpy(type_buf, p, type_len);
    p += type_len;

    type_buf[type_len] = 0;
    if(strcmp(type_buf, "map") != 0) {
        throw std::runtime_error("Data is not a map");
    }

    u32 item_count = * (u32 *) p;
    p += 4;

    std::unordered_map<std::string, const char *> ret;
    for(u32 i = 0; i < item_count; i++) {
        u32 k_len = * (u32 *) p;
        p += 4;
        const char *k = (const char *) p;
        p += k_len + 1;

        u32 v_len = * (u32 *) p;
        p += 4;
        const char *v = (const char *) p;
        p += v_len + 1;

        ret[std::string(k)] = v;
    }

    return ret;
}

struct Task {
    int ep_id;
    Resource call_info;

    Task(int _ep_id, Resource _call_info) {
        ep_id = _ep_id;
        call_info = _call_info;
    }
};

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

        const char * get_remote_addr() {
            return ice_glue_request_get_remote_addr(handle);
        }

        const char * get_method() {
            return ice_glue_request_get_method(handle);
        }

        const char * get_uri() {
            return ice_glue_request_get_uri(handle);
        }

        std::unordered_map<std::string, const char *> get_headers() {
            return read_map(ice_glue_request_get_headers(handle));
        }

        std::unordered_map<std::string, const char *> get_cookies() {
            return read_map(ice_glue_request_get_cookies(handle));
        }

        std::unordered_map<std::string, std::string> get_session_items() {
            auto m = read_map(ice_glue_request_get_session_items(handle));
            std::unordered_map<std::string, std::string> ret;
            for(auto& p : m) {
                // Raw data may become invalid.
                if(p.second) ret[std::move(p.first)] = std::string(p.second);
            }
            return ret;
        }

        void set_session_item(const char *k, const char *v) {
            ice_glue_request_set_session_item(handle, k, v);
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

        Server(uv_loop_t *loop) {
            handle = ice_create_server();
            if(loop) {
                ev_loop = loop;
            } else {
                ev_loop = uv_default_loop();
            }
            uv_async_init(ev_loop, &task_async, ice::dispatch_task);
            task_async.data = (void *) this;

            ice_server_set_custom_app_data(handle, (void *) this);
            ice_server_set_async_endpoint_cb(handle, dispatch_async_endpoint_cb);
        }

        ~Server() {}

        void listen(const char *addr) {
            ice_server_listen(handle, addr);
        }

        void add_endpoint(const char *path, DispatchTarget handler, std::vector<std::string>& flags) {
            Resource ep = ice_server_router_add_endpoint(handle, path);
            for(auto& f : flags) {
                ice_core_endpoint_set_flag(ep, f.c_str(), true);
            }

            int id = ice_core_endpoint_get_id(ep);

            dispatch_table[id] = handler;
        }

        void add_endpoint(const char *path, DispatchTarget handler) {
            std::vector<std::string> flags;
            add_endpoint(path, handler, flags);
        }

        void route_sync(const char *path, SyncDispatchTarget handler, std::vector<std::string>& flags) {
            add_endpoint(path, [=](Request req) {
                Response resp = handler(req);
                resp.send();
            }, flags);
        }

        void route_sync(const char *path, SyncDispatchTarget handler) {
            std::vector<std::string> flags;
            route_sync(path, handler, flags);
        }

        void route_async(const char *path, DispatchTarget handler, std::vector<std::string>& flags) {
            add_endpoint(path, handler, flags);
        }

        void route_async(const char *path, DispatchTarget handler) {
            std::vector<std::string> flags;
            route_async(path, handler, flags);
        }

        void route_threaded(const char *path, SyncDispatchTarget handler, std::vector<std::string>& flags) {
            add_endpoint(path, [=](Request req) {
                std::thread t([=]() {
                    Response resp = handler(req);
                    resp.send();
                });
                t.detach();
            }, flags);
        }
        
        void route_threaded(const char *path, SyncDispatchTarget handler) {
            std::vector<std::string> flags;
            route_threaded(path, handler, flags);
        }

        void disable_request_logging() {
            ice_server_disable_request_logging(handle);
        }

        static void dispatch_async_endpoint_cb(int ep_id, Resource call_info) {
            Server *server = (Server *) ice_core_get_custom_app_data_from_call_info(call_info);
            server -> async_endpoint_cb(ep_id, call_info);
        }

        void async_endpoint_cb(int ep_id, Resource call_info) {
            pending_mutex.lock();
            pending.push_back(Task(ep_id, call_info));
            pending_mutex.unlock();

            uv_async_send(&task_async);
        }

        void run(const char *addr) {
            listen(addr);
            uv_run(ev_loop, UV_RUN_DEFAULT);
        }

        void dispatch_task() {
            pending_mutex.lock();
            std::deque<Task> current_tasks = std::move(pending);
            pending = std::deque<Task>();
            pending_mutex.unlock();

            for(auto& t : current_tasks) {
                Request req(t.call_info);

                auto target = dispatch_table[t.ep_id];
                if(!target) {
                    //std::cerr << "Error: Calling an invalid endpoint: " << ep_id << std::endl;
                    req.create_response().set_status(404).set_body("Invalid endpoint").send();
                    continue;
                }

                target(req);
            }
        }
};

static void dispatch_task(uv_async_t *task_async) {
    Server *server = (Server *) (task_async -> data);
    server -> dispatch_task();
}

} // namespace ice
