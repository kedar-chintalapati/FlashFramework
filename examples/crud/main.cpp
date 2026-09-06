#include <flash/flash.hpp>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct CreateItem {
    [[=flash::min_length(1), =flash::max_length(80)]]
    std::string name;
    [[=flash::minimum(1)]]
    std::uint32_t quantity{};
};

struct PatchItem {
    [[=flash::min_length(1), =flash::max_length(80)]]
    std::optional<std::string> name;
    [[=flash::minimum(1)]]
    std::optional<std::uint32_t> quantity;
};

struct Item {
    std::uint64_t id{};
    std::string name;
    std::uint32_t quantity{};
};

enum class item_error {
    not_found,
};

template <>
inline constexpr auto flash::error_map<item_error> = flash::errors(
    flash::map<item_error::not_found>(
        flash::status::not_found, "item_not_found", "Item not found"));

class ItemStore {
public:
    Item create(CreateItem input) {
        std::lock_guard lock{mutex_};
        Item item{next_id_++, std::move(input.name), input.quantity};
        items_.push_back(item);
        return item;
    }

    std::expected<Item, item_error> find(std::uint64_t id) {
        std::lock_guard lock{mutex_};
        const auto found = find_locked(id);
        if (found == items_.end()) {
            return std::unexpected{item_error::not_found};
        }
        return *found;
    }

    std::expected<Item, item_error> replace(
        std::uint64_t id, CreateItem input) {
        std::lock_guard lock{mutex_};
        const auto found = find_locked(id);
        if (found == items_.end()) {
            return std::unexpected{item_error::not_found};
        }
        found->name = std::move(input.name);
        found->quantity = input.quantity;
        return *found;
    }

    std::expected<Item, item_error> patch(
        std::uint64_t id, PatchItem input) {
        std::lock_guard lock{mutex_};
        const auto found = find_locked(id);
        if (found == items_.end()) {
            return std::unexpected{item_error::not_found};
        }
        if (input.name) {
            found->name = std::move(*input.name);
        }
        if (input.quantity) {
            found->quantity = *input.quantity;
        }
        return *found;
    }

    std::expected<void, item_error> erase(std::uint64_t id) {
        std::lock_guard lock{mutex_};
        const auto found = find_locked(id);
        if (found == items_.end()) {
            return std::unexpected{item_error::not_found};
        }
        items_.erase(found);
        return {};
    }

private:
    using iterator = std::vector<Item>::iterator;

    iterator find_locked(std::uint64_t id) {
        return std::ranges::find(items_, id, &Item::id);
    }

    std::mutex mutex_;
    std::vector<Item> items_;
    std::uint64_t next_id_{1};
};

namespace api {

[[=flash::get("/items/{id}")]]
std::expected<Item, item_error> get_item(
    std::uint64_t id, flash::state<ItemStore>& store) {
    return store->find(id);
}

[[=flash::post("/items")]]
flash::created<Item> create_item(
    CreateItem input, flash::state<ItemStore>& store) {
    auto item = store->create(std::move(input));
    return {item, "/items/" + std::to_string(item.id)};
}

[[=flash::put("/items/{id}")]]
std::expected<Item, item_error> replace_item(
    std::uint64_t id, CreateItem input, flash::state<ItemStore>& store) {
    return store->replace(id, std::move(input));
}

[[=flash::patch("/items/{id}")]]
std::expected<Item, item_error> patch_item(
    std::uint64_t id, PatchItem input, flash::state<ItemStore>& store) {
    return store->patch(id, std::move(input));
}

[[=flash::delete_("/items/{id}")]]
std::expected<void, item_error> delete_item(
    std::uint64_t id, flash::state<ItemStore>& store) {
    return store->erase(id);
}

} // namespace api

int main() {
    ItemStore store;
    return flash::serve<^^api>({.port = 8080, .workers = 2}, store);
}
