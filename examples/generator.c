#include <stdio.h>
#include "gt.h"

void generator(gt_ctx_t* ctx, void* arg) {
    uint32_t limit = *(uint32_t*)arg;
    for(uint32_t i = 0; i < limit; i++) {
        gt_thread_resume(ctx, gt_caller(ctx), &i);
    }
}

int main(void) {
    gt_ctx_t* ctx = gt_ctx_create();
    gt_thread_t* thread = gt_thread_create_child(ctx, generator);

    uint32_t limit = 20;
    while(1) {
        uint32_t* i = gt_thread_resume(ctx, thread, &limit);
        if(i == NULL) {
            break;
        }
        printf("%u ", *i); 
    }
    gt_ctx_free(ctx);
}
