// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#ifdef _WIN32
#    include <windows.h>
#    if !defined(ERROR)
#        error Expected the unchanged Windows ERROR macro before the sidecar header
#    endif
#endif
#include "../../tools/tape-inspect/sidecar_time.hpp"

/** Compile and execute a Windows-header-first consumer without changing caller macros.
 * @return Zero when the scanner API remains usable after windows.h.
 */
int main() {
    using namespace org::ttldtor::qdcxx::tape_inspect;
    SidecarTime scanner(static_cast<SidecarFormat>(99));

#ifdef _WIN32
    static_assert(ERROR == 0);
#endif

    return scanner.feed({}).status == SidecarStatus::FAILED ? 0 : 1;
}
