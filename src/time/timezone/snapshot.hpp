#pragma once

#include <cstddef>
#include <string>

namespace org::ttldtor::qdcxx::time::detail::provider::input {

/** Exact approved source bytes for finite admission and the private native parser. */
struct FileBytes {
    const char *name;
    const unsigned char *bytes;
    std::size_t size;
};

extern const FileBytes APPROVED_FILES[];
extern const std::size_t APPROVED_FILE_COUNT;

/** Return the retained admitted bytes or null for an excluded legacy file.
 * @param name Upstream's fixed file name.
 * @return Immutable retained bytes; owned until process shutdown.
 */
const std::string *snapshotFile(const char *name);

/** Charge one native model construction/insertion before container growth.
 * Throws on the finite cumulative model-work limit; discarded work is not refunded.
 */
void admitNativeModel();

} // namespace org::ttldtor::qdcxx::time::detail::provider::input
