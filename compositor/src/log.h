#pragma once
#include <cstdio>

#define LOGI(...) (std::fprintf(stderr, "[oasis] " __VA_ARGS__), std::fputc('\n', stderr))
#define LOGE(...) (std::fprintf(stderr, "[oasis:err] " __VA_ARGS__), std::fputc('\n', stderr))
