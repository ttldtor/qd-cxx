// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <iosfwd>

namespace org::ttldtor::qdcxx::tape_inspect {
/** Print the owning source profile's options without opening or discovering files.
 * @param output Borrowed help destination, flushed by the caller.
 */
void sourceCliHelp(std::ostream &output);
/** Execute the explicit owning source profile with native process arguments.
 * Arguments are borrowed synchronously. Windows obtains the original UTF-16 arguments from the CRT.
 * Names are checked before copying and no callback survives.
 * @param argc Number of arguments, including executable name.
 * @param argv Borrowed native arguments.
 * @param output Borrowed stdout destination through the final fallible flush.
 * @param diagnostic Borrowed stderr destination.
 * @return Documented process exit, with zero requiring complete owning inspection.
 */
int runSourceCli(int argc, const char *const *argv, std::ostream &output, std::ostream &diagnostic);

} // namespace org::ttldtor::qdcxx::tape_inspect
