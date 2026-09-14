#include "tests/support/test_assertions.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <filesystem>
#include <iostream>

int main()
{
    using elysia::tests::require;
    require(SDL_Init(0),"SDL initialization");
    require(MIX_Init(),"mixer decoder initialization");
    require(TTF_Init(),"font decoder initialization");
    int images=0,audio_files=0,fonts=0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(ELYSIA_SOURCE_DIR)/"assets"))
    {
        if (!entry.is_regular_file()) continue;
        const auto path=entry.path().string();
        const auto extension=entry.path().extension().string();
        if (extension==".png" || extension==".jpg" || extension==".jpeg")
        {
            SDL_Surface* surface=IMG_Load(path.c_str());
            require(surface != nullptr,path.c_str());
            SDL_DestroySurface(surface);
            ++images;
        }
        else if (extension==".wav" || extension==".ogg" || extension==".mp3")
        {
            // Exercise both cache policies against every audio source.
            for (bool predecode : {false,true})
            {
                MIX_Audio* audio=MIX_LoadAudio(nullptr,path.c_str(),predecode);
                require(audio != nullptr,path.c_str());
                MIX_DestroyAudio(audio);
            }
            ++audio_files;
        }
        else if (extension==".ttf" || extension==".otf")
        {
            TTF_Font* font=TTF_OpenFont(path.c_str(),20);
            require(font != nullptr,path.c_str());
            TTF_CloseFont(font);
            ++fonts;
        }
    }
    require(images>0 && audio_files>0 && fonts>0,"asset scan must exercise all resource families");
    std::cout << "Decoded " << images << " images, " << audio_files << " audio files, " << fonts << " fonts\n";
    TTF_Quit(); MIX_Quit(); SDL_Quit();
}
