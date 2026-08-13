#if (defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)) \
    && (defined(__GNUC__) || defined(__clang__))
int main() {
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") ? 0 : 1;
}
#else
int main() {
    return 1;
}
#endif
