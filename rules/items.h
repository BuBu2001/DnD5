// rules/items.h — предмет как набор компонентов данных (FR-E121, E-D09, DOC-01 FR-K80…FR-K83).
// Хардкод предметов запрещён (NFR-E13): предмет описывается данными JSON формата
// п. Г.9 ТЗ DOC-04 — массив components с типами Weapon / OnHitEffect / AttunementRequired.
// Настройка (attunement): не более трёх предметов (формула 19 п. Г.13, AC-K18),
// занимает короткий отдых (DOC-01 FR-K81).
// ВАЖНО (E-D01): файл не включает заголовки engine/*.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "rules/damage.h"
#include "rules/modifier_stack.h"

namespace rules {

using ItemId = std::string; // "Item_Longsword" (п. 4.5.4)

enum class ItemComponentType : std::uint8_t {
    Weapon,             // base_damage, damage_type, свойства оружия
    OnHitEffect,        // эффект при попадании (AddDamage, AddCondition…)
    AttunementRequired, // требуется настройка (лимит 3, AC-K18)
    Armor,              // категория брони и базовое AC (FR-E112)
    Modifier            // плоские модификаторы при экипировке (FR-K83)
};

enum class WeaponProperty : std::uint8_t {
    None, Finesse, Light, Heavy, TwoHanded, Ranged, Thrown, Versatile, Reach
};

// Компонент предмета: ровно одна структура на тип (данные, без поведения, E-D09)
struct ItemComponent {
    ItemComponentType type = ItemComponentType::Weapon;

    // Weapon
    std::string base_damage;             // "1d8"
    DamageType damage_type = DamageType::Slashing;
    std::vector<WeaponProperty> properties;

    // OnHitEffect
    std::string effect;                  // "AddDamage" | "AddCondition"
    std::string damage;                  // "1d4"
    DamageType effect_damage_type = DamageType::Fire;
    std::string condition_id;            // "Condition_Prone"

    // Armor
    std::uint8_t armor_base_ac = 0;
    std::uint8_t armor_category = 0;     // rules::ArmorCategory as uint8 (None/Light/Medium/Heavy)

    // Modifier (FR-K83): модификаторы, добавляемые при экипировке
    std::vector<Modifier> modifiers;
};

struct ItemDef {
    ItemId id;
    std::string name;
    std::string description;
    std::vector<ItemComponent> components;

    const ItemComponent* find(ItemComponentType t) const {
        const auto it = std::find_if(components.begin(), components.end(),
                                     [t](const ItemComponent& c) { return c.type == t; });
        return it == components.end() ? nullptr : &*it;
    }

    bool requires_attunement() const { return find(ItemComponentType::AttunementRequired) != nullptr; }
};

// Лимит настройки: attunedItems.length <= 3 (формула 19 п. Г.13)
inline constexpr std::size_t kMaxAttunedItems = 3;

// Набор настроенных предметов. Настройка занимает короткий отдых (AC-K18):
// запрос настройки помещается в очередь и применяется на rest().
class AttunementSet {
public:
    bool is_attuned(const ItemId& id) const {
        return std::find(attuned_.begin(), attuned_.end(), id) != attuned_.end();
    }

    std::size_t count() const { return attuned_.size(); }
    bool is_full() const { return attuned_.size() >= kMaxAttunedItems; }

    // Запрос настройки: невозможно настроить четвёртый предмет (AC-K18)
    bool request_attune(const ItemId& id) {
        if (is_attuned(id)) return false;               // повторная настройка не нужна
        if (is_full()) return false;                    // лимит 3
        if (std::find(pending_.begin(), pending_.end(), id) != pending_.end()) return false;
        pending_.push_back(id);
        return true;
    }

    // Короткий отдых: применяет отложенные запросы в порядке поступления
    std::size_t rest() {
        std::size_t applied = 0;
        for (const auto& id : pending_) {
            if (is_full() || is_attuned(id)) continue;
            attuned_.push_back(id);
            ++applied;
        }
        pending_.clear();
        return applied;
    }

    // Снятие настройки — немедленно (снятие экипировки)
    bool unattune(const ItemId& id) {
        const auto it = std::find(attuned_.begin(), attuned_.end(), id);
        if (it == attuned_.end()) return false;
        attuned_.erase(it);
        return true;
    }

    const std::vector<ItemId>& attuned() const { return attuned_; }
    const std::vector<ItemId>& pending() const { return pending_; }

private:
    std::vector<ItemId> attuned_;
    std::vector<ItemId> pending_;
};

// Инвентарь: лимит слотами (DOC-01 FR-K80, OQ-03: базовое значение — 20 слотов).
// Вес и перегрузка не реализуются (FR-K80, AC-K19).
inline constexpr std::size_t kDefaultInventorySlots = 20;

class Inventory {
public:
    explicit Inventory(std::size_t slots = kDefaultInventorySlots) : slots_(slots) {}

    std::size_t capacity() const { return slots_; }
    std::size_t used() const { return items_.size(); }
    bool is_full() const { return items_.size() >= slots_; }

    bool add(const ItemId& id) {
        if (is_full()) return false;
        items_.push_back(id);
        return true;
    }

    bool remove(const ItemId& id) {
        const auto it = std::find(items_.begin(), items_.end(), id);
        if (it == items_.end()) return false;
        items_.erase(it);
        return true;
    }

    bool contains(const ItemId& id) const {
        return std::find(items_.begin(), items_.end(), id) != items_.end();
    }

    std::size_t count_of(const ItemId& id) const {
        return static_cast<std::size_t>(std::count(items_.begin(), items_.end(), id));
    }

    const std::vector<ItemId>& items() const { return items_; }

private:
    std::size_t slots_;
    std::vector<ItemId> items_;
};

// Экипировка: операции обязаны быть симметричными и идемпотентными (FR-K83, AC-K20) —
// два цикла «надеть/снять» не меняют характеристики.
class Equipment {
public:
    bool equip(const ItemDef& item) {
        if (is_equipped(item.id)) return false;      // идемпотентность
        if (item.requires_attunement() && !attunement_.is_attuned(item.id)) return false;
        equipped_.push_back(item.id);
        for (const auto& c : item.components) {
            if (c.type == ItemComponentType::Modifier) {
                for (const auto& m : c.modifiers) active_modifiers_.push_back(m);
            }
        }
        return true;
    }

    // Снятие убирает РОВНО те же модификаторы, что были добавлены (AC-K20)
    bool unequip(const ItemDef& item) {
        const auto it = std::find(equipped_.begin(), equipped_.end(), item.id);
        if (it == equipped_.end()) return false;
        equipped_.erase(it);
        for (const auto& c : item.components) {
            if (c.type != ItemComponentType::Modifier) continue;
            for (const auto& m : c.modifiers) remove_one_modifier(m.source_id);
        }
        return true;
    }

    bool is_equipped(const ItemId& id) const {
        return std::find(equipped_.begin(), equipped_.end(), id) != equipped_.end();
    }

    const std::vector<Modifier>& modifiers() const { return active_modifiers_; }
    const std::vector<ItemId>& equipped() const { return equipped_; }

    AttunementSet& attunement() { return attunement_; }
    const AttunementSet& attunement() const { return attunement_; }

private:
    void remove_one_modifier(const std::string& source_id) {
        const auto it = std::find_if(active_modifiers_.begin(), active_modifiers_.end(),
                                     [&](const Modifier& m) { return m.source_id == source_id; });
        if (it != active_modifiers_.end()) active_modifiers_.erase(it);
    }

    AttunementSet attunement_;
    std::vector<ItemId> equipped_;
    std::vector<Modifier> active_modifiers_;
};

// Разбор записи костей "NdM" (например "1d8", "2d6") без внешних зависимостей
struct DiceNotation {
    int count = 0;
    int sides = 0;
    bool valid = false;
};

DiceNotation parse_dice_notation(const std::string& text);

} // namespace rules
