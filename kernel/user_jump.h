extern __attribute__((noreturn)) void jump_to_user(void* rip, void* rsp);
extern __attribute__((noreturn)) void context_switch(void* iframe);
