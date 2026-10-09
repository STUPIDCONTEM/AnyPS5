#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <limits>

extern "C" {
float APS5_VABI _FSinh_nid_postfix(float, float);
float APS5_VABI _FCosh_nid_postfix(float, float);
}

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "Scaled hyperbolic: %s\n", message);
    return condition;
}

bool Close(float actual, float expected) {
    return std::isfinite(actual) && std::fabs(actual - expected) <= std::fabs(expected) * 0x1p-22f;
}

bool CheckFinite() {
    struct Case { float argument; float scale; float sine; float cosine; };
    constexpr std::array cases {
        Case {0.25f, 0x1p-3f, 0x1.02accd9d08102p-5f, 0x1.080ab05ca6146p-3f},
        Case {1.0f, 0x1p-4f, 0x1.2cd9fc44eb982p-4f, 0x1.8b07551d9f550p-4f},
        Case {90.0f, 0x1p-10f, 0x1.cb108ffbec164p+118f, 0x1.cb108ffbec164p+118f},
        Case {100.0f, 0x1p-40f, 0x1.3494a9b171bf5p+103f, 0x1.3494a9b171bf5p+103f},
        Case {150.0f, 0x1p-140f, 0x1.52cac29822593p+75f, 0x1.52cac29822593p+75f},
        Case {192.0f, 0x1p-149f, 0x1.ff18562cc483ep+126f, 0x1.ff18562cc483ep+126f}
    };
    bool correct = true;
    for (const auto& value : cases) {
        for (const float argumentSign : {-1.0f, 1.0f}) {
            for (const float scaleSign : {-1.0f, 1.0f}) {
                errno = 123;
                const float sine = _FSinh_nid_postfix(argumentSign * value.argument, scaleSign * value.scale);
                correct &= Check(Close(sine, argumentSign * scaleSign * value.sine), "finite scaled sinh");
                correct &= Check(errno == 123, "finite sinh preserves errno");
                const float cosine = _FCosh_nid_postfix(argumentSign * value.argument, scaleSign * value.scale);
                correct &= Check(Close(cosine, scaleSign * value.cosine), "finite scaled cosh");
                correct &= Check(errno == 123, "finite cosh preserves errno");
            }
        }
    }
    return correct;
}

bool CheckSpecial() {
    bool correct = true;
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (const float argument : {0.0f, -0.0f, 100.0f, -100.0f, 1.0e30f, -1.0e30f}) {
        for (const float scale : {0.0f, -0.0f}) {
            errno = 123;
            const float sine = _FSinh_nid_postfix(argument, scale);
            const float cosine = _FCosh_nid_postfix(argument, scale);
            correct &= Check(sine == 0.0f && std::signbit(sine) == (std::signbit(argument) != std::signbit(scale)), "sinh zero scale sign");
            correct &= Check(cosine == 0.0f && std::signbit(cosine) == std::signbit(scale), "cosh zero scale sign");
            correct &= Check(errno == 123, "zero scale preserves errno");
        }
    }
    for (const float argument : {infinity, -infinity}) {
        for (const float scale : {0.0f, -0.0f, 0.5f, -0.5f}) {
            errno = 123;
            const float sine = _FSinh_nid_postfix(argument, scale);
            const float cosine = _FCosh_nid_postfix(argument, scale);
            if (scale == 0.0f) {
                correct &= Check(sine == 0.0f && std::signbit(sine) == (std::signbit(argument) != std::signbit(scale)), "infinite sinh zero scale");
            } else {
                correct &= Check(sine == argument, "infinite sinh argument");
            }
            correct &= Check(cosine == argument, "infinite cosh argument");
            correct &= Check(errno == 123, "infinite argument preserves errno");
        }
    }
    for (const float scale : {0.0f, -0.0f, 0.5f, -0.5f}) {
        correct &= Check(std::isnan(_FSinh_nid_postfix(nan, scale)), "sinh NaN argument");
        correct &= Check(std::isnan(_FCosh_nid_postfix(nan, scale)), "cosh NaN argument");
        correct &= Check(_FSinh_nid_postfix(0.0f, scale) == 0.0f, "sinh zero argument");
        correct &= Check(_FCosh_nid_postfix(0.0f, scale) == scale, "cosh zero argument");
    }
    for (const float argumentSign : {-1.0f, 1.0f}) {
        for (const float scaleSign : {-1.0f, 1.0f}) {
            errno = 123;
            const float sine = _FSinh_nid_postfix(argumentSign * 0x1p-149f, scaleSign * 0x1p-149f);
            correct &= Check(sine == 0.0f && std::signbit(sine) == (argumentSign != scaleSign), "tiny sinh product zero sign");
            correct &= Check(errno == 123, "tiny sinh product preserves errno");
        }
    }
    for (const float argumentSign : {-1.0f, 1.0f}) {
        for (const float scaleSign : {-1.0f, 1.0f}) {
            for (const float argument : {100.0f, 193.0f}) {
                const float scale = argument == 100.0f ? 1.0f : 0x1p-149f;
                errno = 123;
                const float sine = _FSinh_nid_postfix(argumentSign * argument, scaleSign * scale);
                correct &= Check(std::isinf(sine) && std::signbit(sine) == (argumentSign != scaleSign), "sinh true overflow sign");
                correct &= Check(errno == ERANGE, "sinh true overflow errno");
                errno = 123;
                const float cosine = _FCosh_nid_postfix(argumentSign * argument, scaleSign * scale);
                correct &= Check(std::isinf(cosine) && std::signbit(cosine) == (scaleSign < 0.0f), "cosh true overflow sign");
                correct &= Check(errno == ERANGE, "cosh true overflow errno");
            }
        }
    }
    return correct;
}

}

int main() {
    const bool finite = CheckFinite();
    const bool special = CheckSpecial();
    return finite && special ? 0 : 1;
}
