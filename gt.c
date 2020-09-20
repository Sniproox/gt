#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "gt.h"

#define PTR   uintptr_t
#define PTRSZ sizeof(PTR)

#if defined(__i386__)
#elif defined(__x86_64__)
    #define GT64 1
#else
# error Unsupported architecture
#endif

typedef struct {
#ifdef GT64
    union {
        uint64_t rsp;
        uintptr_t stack;
    };
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
#else
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
    union {
        uint32_t ebp;
        uintptr_t stack;
    };
    uint32_t esp;
#endif
} gt_regs_t;

//this is just a marker type.
struct __gt_tls {};

typedef struct {
    uint32_t key;
    void* value;
} gt_tls_entry_t;

typedef struct {
    uint32_t index;
    bool present;
} gt_tls_find_result_t;

struct __gt_thread {
    gt_regs_t regs;
    gt_thread_state_t state;
    uint8_t* stack;
    struct __gt_thread* caller;
    struct {
        destructor_t* list;
        uint32_t count;
    } dtors;
    struct {
        gt_tls_entry_t* data;
        uint32_t count;
    } tls;
};

struct __gt_ctx {
    gt_thread_t* current;
    gt_thread_t* root;
    void* buffer;
    size_t stacksize;
    uint32_t ntls;
};

//abandon all hope, ye who enter here
extern void gt_thread_switch(gt_thread_t* current, gt_thread_t* to);
extern void gt_die();
#ifdef GT64
extern void gt_start_thread();
#endif

void gt_do_return(gt_ctx_t* ctx, gt_thread_t* thread) {
    (void)ctx;
    thread->state = gt_state_dead;
    ctx->buffer = NULL;
    gt_thread_switch(thread, thread->caller);
}

void gt_thread_destroy(gt_thread_t* thread) {
    for(uint32_t i = 0; i < thread->dtors.count; i++) {
        destructor_t d = thread->dtors.list[i];
        d.fn(d.arg);
    }
    free(thread->dtors.list);
    thread->dtors.list = NULL;
    thread->dtors.count = 0;

    free(thread->tls.data);
    thread->tls.data = NULL;
    thread->tls.count = 0;

    free(thread->stack);
    thread->stack = NULL;
}

gt_ctx_t* gt_ctx_create() {
    gt_ctx_t* ctx = malloc(sizeof(gt_ctx_t));
    if(ctx == NULL) {
        return NULL;
    }
    ctx->stacksize = 131072;
    ctx->ntls = 0;
    ctx->current = ctx->root = malloc(sizeof(gt_thread_t));
    if(ctx->current == NULL) {
        free(ctx);
        return NULL;
    }
    ctx->current->stack = NULL;
    ctx->current->dtors.list = NULL;
    ctx->current->dtors.count = 0;
    ctx->current->tls.data = NULL;
    ctx->current->tls.count = 0;
    return ctx;
}

void gt_ctx_free(gt_ctx_t* ctx) {
    gt_thread_free(ctx->root);
    free(ctx);
}

void gt_ctx_set_stack_size(gt_ctx_t* ctx, size_t size) {
    if(size >= 4096) {
        ctx->stacksize = size;
    }
}

#ifndef GT64
void gt_start_thread(gt_start_fn fn, gt_ctx_t* ctx) {
    ctx->current->state = gt_state_alive;
    fn(ctx, ctx->buffer);
}
#endif

gt_thread_t* gt_thread_create(gt_ctx_t* ctx, gt_start_fn fn) {
    gt_thread_t* thread = malloc(sizeof(gt_thread_t));
    if(thread == NULL) {
        return NULL;
    }
    thread->dtors.list = NULL;
    thread->dtors.count = 0;
    thread->tls.data = NULL;
    thread->tls.count = 0;

    size_t stacksize = ctx->stacksize;
    uint8_t* stack = memalign(64, sizeof(uint8_t) * stacksize);
    if(stack == NULL) {
        free(thread);
        return NULL;
    }
    memset(&thread->regs, 0, sizeof(gt_regs_t));
    int pos = 0;
#define PUSH(x) do { *(PTR*)(&stack[stacksize - PTRSZ*(++pos)]) = (PTR)(x); } while(0)
#ifdef GT64
    PUSH(0); //align stack
    PUSH(ctx);
    PUSH(thread);
    PUSH(gt_die);
    PUSH(ctx);
    PUSH(fn);
    PUSH(gt_start_thread);
#else
    PUSH(ctx);
    PUSH(thread);
    PUSH(ctx);
    PUSH(fn);
    PUSH(gt_die);
    PUSH(gt_start_thread);
#endif
    thread->regs.stack = (PTR)&stack[stacksize - PTRSZ*pos];

#ifndef GT64
    thread->regs.esp = thread->regs.ebp;
#endif

    thread->stack = stack;
    thread->state = gt_state_new;
    return thread;
}

gt_thread_t* gt_thread_create_child(gt_ctx_t* ctx, gt_start_fn fn) {
    gt_thread_t* thread = gt_thread_create(ctx, fn);
    if(thread == NULL) {
        return NULL;
    }
    destructor_t dtor = {
        .fn = (void (*)(void*))gt_thread_free,
        .arg = thread
    };
    if(!gt_register_destructor(ctx, dtor)) {
        gt_thread_free(thread);
        return NULL;
    }
    return thread;
}

void gt_thread_free(gt_thread_t* thread) {
    gt_thread_destroy(thread);
    free(thread);
}

gt_tls_t* gt_tls_new(gt_ctx_t* ctx) {
    return (gt_tls_t*)(size_t)(++ctx->ntls);
}

static gt_tls_find_result_t gt_tls_find_entry(
        const gt_tls_entry_t* entries, uint32_t size, uint32_t key);

static inline void** gt_tls_get_location(gt_ctx_t* ctx, gt_tls_t* tls) {
    gt_thread_t* t = ctx->current;
    uint32_t key = (uint32_t)(size_t)tls;
    gt_tls_find_result_t pos = gt_tls_find_entry(t->tls.data, t->tls.count, key);

    if(pos.present) {
        return &t->tls.data[pos.index].value;
    }
    gt_tls_entry_t* new = realloc(t->tls.data, t->tls.count + 1);
    if(new == NULL) {
        return NULL;
    }
    memmove(&new[pos.index+1], &new[pos.index],
            sizeof(gt_tls_entry_t) * (t->tls.count - pos.index));

    new[pos.index] = (gt_tls_entry_t) {
        .key = key,
        .value = NULL
    };

    t->tls.data = new;
    t->tls.count++;
    return &t->tls.data[pos.index].value;
}

bool gt_tls_get(gt_ctx_t* ctx, gt_tls_t* tls, void** dest) {
    void** slot = gt_tls_get_location(ctx, tls);
    if(slot == NULL) {
        return false;
    }
    *dest = *slot;
    return true;
}

bool gt_tls_set(gt_ctx_t* ctx, gt_tls_t* tls, void* value, void** old) {
    void** slot = gt_tls_get_location(ctx, tls);
    if(slot == NULL) {
        return false;
    }
    if(old != NULL) {
        *old = *slot;
    }
    *slot = value;
    return true;
}

void gt_tls_free(gt_ctx_t* ctx, gt_tls_t* tls) {
    //nothing to do
    (void)ctx;
    (void)tls;
}

gt_thread_state_t gt_thread_state(gt_thread_t* thread) {
    return thread->state;
}

void* gt_thread_resume(gt_ctx_t* ctx, gt_thread_t* to, void* arg) {
    if(to->state == gt_state_dead) {
        return NULL;
    }
    gt_thread_t* curr = ctx->current;
    gt_thread_t* currcall = curr->caller;
    
    to->caller = curr;
    ctx->current = to;

    ctx->buffer = arg;

    gt_thread_switch(curr, to);
    if(to->state == gt_state_dead) {
        gt_thread_destroy(to);
    }

    curr->caller = currcall;

    return ctx->buffer;
}

void* gt_thread_yield(gt_ctx_t* ctx, void* arg) {
    return gt_thread_resume(ctx, gt_caller(ctx), arg);
}

gt_thread_t* gt_caller(gt_ctx_t* ctx) {
    return ctx->current->caller;
}

gt_thread_t* gt_current(gt_ctx_t* ctx) {
    return ctx->current;
}

bool gt_register_destructor(gt_ctx_t* ctx, destructor_t destructor) {
    gt_thread_t* t = ctx->current;
    destructor_t* d = realloc(t->dtors.list, t->dtors.count + 1);
    if(d == NULL) {
        return false;
    }
    d[t->dtors.count] = destructor;
    t->dtors.count++;
    t->dtors.list = d;
    return true;
}

gt_tls_find_result_t gt_tls_find_entry(const gt_tls_entry_t* tls, uint32_t size, uint32_t key) {
    if(size == 0) {
        return (gt_tls_find_result_t) {
            .index = 0,
            .present = false
        };
    }
    uint32_t mid = 0;
    uint32_t left = 0;
    uint32_t right = size;
    int cmp;

    while (left < right) {
        mid = (left + right) / 2;
        cmp = tls[mid].key - key;
        if (cmp < 0) {
            right = mid;
        } else if (cmp > 0) {
            left = mid+1;
        } else {
            break;
        }
    }

    if(cmp != 0) mid++;
    gt_tls_find_result_t res = {
        .index = mid,
        .present = cmp == 0
    };
    return res;
}

