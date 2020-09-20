#include <stdio.h>
#include "gt.h"

int main(void) {
    gt_ctx_t* ctx = gt_ctx_create();
    gt_thread_t* self = gt_current(ctx);
    char* s = gt_thread_resume(ctx, self, "Hello World!");
    printf("%s\n", s);
    gt_thread_switch(self, self);
    printf("%s\n", s);
    gt_ctx_free(ctx);
}
