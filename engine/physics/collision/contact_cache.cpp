#include "contact_cache.h"

#include <algorithm>

namespace elysia::physics
{
void ContactCache::update(
    std::span<const CollisionContact> current_contacts,
    std::vector<CollisionEvent>& out_events)
{
    out_events.clear();
    std::size_t previous_index = 0;
    std::size_t current_index = 0;
    while (previous_index < _contacts.size()
        || current_index < current_contacts.size())
    {
        if (previous_index >= _contacts.size())
        {
            out_events.push_back(CollisionEvent{
                CollisionEventPhase::Begin,
                current_contacts[current_index++]
            });
            continue;
        }
        if (current_index >= current_contacts.size())
        {
            out_events.push_back(CollisionEvent{
                CollisionEventPhase::End,
                _contacts[previous_index++]
            });
            continue;
        }

        const CollisionContact& previous = _contacts[previous_index];
        const CollisionContact& current = current_contacts[current_index];
        if (previous.pair < current.pair)
        {
            out_events.push_back(CollisionEvent{CollisionEventPhase::End, previous});
            ++previous_index;
        }
        else if (current.pair < previous.pair)
        {
            out_events.push_back(CollisionEvent{CollisionEventPhase::Begin, current});
            ++current_index;
        }
        else
        {
            out_events.push_back(CollisionEvent{CollisionEventPhase::Stay, current});
            ++previous_index;
            ++current_index;
        }
    }
    _contacts.assign(current_contacts.begin(), current_contacts.end());
}

void ContactCache::collect_contacts(
    CollisionTarget target,
    std::vector<CollisionContact>& out_contacts) const
{
    out_contacts.clear();
    if (!target.is_valid())
        return;
    for (const CollisionContact& contact : _contacts)
    {
        if (contact.pair.first == target || contact.pair.second == target)
            out_contacts.push_back(contact);
    }
}

void ContactCache::remove_target(CollisionTarget target) noexcept
{
    std::erase_if(_contacts, [target](const CollisionContact& contact)
    {
        return contact.pair.first == target || contact.pair.second == target;
    });
}

void ContactCache::remove_tiles() noexcept
{
    std::erase_if(_contacts, [](const CollisionContact& contact)
    {
        return contact.pair.first.kind == CollisionTargetKind::Tile
            || contact.pair.second.kind == CollisionTargetKind::Tile;
    });
}

void ContactCache::clear() noexcept
{
    _contacts.clear();
}

std::span<const CollisionContact> ContactCache::contacts() const noexcept
{
    return _contacts;
}
}
