#include <stdio.h>
#include "gt.h"

gt_thread_t* ping;
gt_thread_t* pong;

void ping_fn(gt_ctx_t* ctx, void* arg) {
    (void)arg;
    while(1) {
        printf("ping!\n");
        gt_thread_switch(ping, pong);
    }
}

void pong_fn(gt_ctx_t* ctx, void* arg) {
    (void)arg;
    while(1) {
        printf("pong!\n");
        gt_thread_switch(pong, ping);
    }
}

int main(void) {
    gt_ctx_t* ctx = gt_ctx_create();

    ping = gt_thread_create_child(ctx, ping_fn);
    pong = gt_thread_create_child(ctx, pong_fn);

    gt_thread_switch(gt_current(ctx), ping);

    gt_ctx_free(ctx);
}
