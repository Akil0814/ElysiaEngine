#pragma once

#include "engine/builtin/audio/builtin_music_player.h"

namespace elysia::builtin
{
class BuiltinMusicPlayerTestAccess
{
public:
    [[nodiscard]] static MIX_Track* track()
    {
        return BuiltinMusicPlayer::instance()->_track;
    }
};
}
