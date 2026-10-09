#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI getargc_nid_postfix();
const char** APS5_VABI getargv_nid_postfix();
}

int main(int argc, char**) {
    const std::vector<std::string> expected = argc == 1 ? std::vector<std::string>{} :
        std::vector<std::string>{"plain", "with space", "", "quote\"inside", "trailing\\", "\xe9\x9b\xaa", "a;b", "*", "\xf0\x9f\x8e\xae"};
    const int count = getargc_nid_postfix();
    if (count != static_cast<int>(expected.size()) + 1) {
        std::fprintf(stderr, "Argument count: got %d, expected %zu\n", count, expected.size() + 1);
        return 1;
    }
    const char** values = getargv_nid_postfix();
    if (!values || !values[0] || !*values[0] || values[count] != nullptr) return 2;
    for (int index = 1; index < count; ++index) {
        if (!values[index] || expected[index - 1] != values[index]) {
            std::fprintf(stderr, "Argument %d differs\n", index);
            return 3;
        }
    }
    if (getargc_nid_postfix() != count || getargv_nid_postfix() != values) return 4;
    return 0;
}
