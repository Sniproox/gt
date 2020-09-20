#include <stdio.h>
#include "gt.h"

void thread_2_dtor(void* arg) { printf("inside destructor, n = %d\n", *(int*)arg); }

void thread_2(gt_ctx_t* ctx, void* arg) {
    printf("[t2] %s\n", (char*)arg);
    char buffer[100];
    int n = 10;
    destructor_t dtor = { .fn = thread_2_dtor, .arg = &n };
    gt_register_destructor(ctx, dtor);
    for(int i = 0; i < 10; i++) {
        snprintf(buffer, sizeof(buffer), "inner thread iteration #%d", i);
        gt_thread_resume(ctx, gt_caller(ctx), buffer);
    }
}

void thread_1(gt_ctx_t* ctx, void* arg) {
    printf("[t1] %s\n", (char*)arg);
    gt_thread_t* t = gt_thread_create_child(ctx, thread_2);
    char* resp;
    do {
        resp = gt_thread_resume(ctx, t, "some string");
        printf("[t1] got '%s' from t2\n", resp);
    } while(resp != NULL);
}

int main(void) {
    gt_ctx_t* ctx = gt_ctx_create();
    gt_thread_t* t = gt_thread_create_child(ctx, thread_1);
    gt_thread_resume(ctx, t, "Hello World");
    gt_ctx_free(ctx);
}
