#include <flash/error_mapping.hpp>

enum class service_error {
    missing,
    busy,
};

template <>
inline constexpr auto flash::error_map<service_error> = flash::errors(
    flash::map<service_error::missing>(
        flash::status::not_found, "service_missing", "Service missing"));

static_assert(flash::error_mapping_validated<service_error>);
