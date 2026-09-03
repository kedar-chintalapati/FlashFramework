#include <flash/version.hpp>

namespace flash {

std::string_view runtime_version() noexcept {
    return version;
}

} // namespace flash

