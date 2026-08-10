#define SDL_MAIN_HANDLED
#include "engine/application/application.h"
#include "game/application/moonline_game_module.h"

#include <cstdlib>

int main(int argc, char** argv)
{
	moonline::application::MoonlineGameModule game_module;
	
	if (!ELYSIA_INIT_APP(argc, argv, game_module))
		return EXIT_FAILURE;
	
	return ELYSIA_RUN_APP == elysia::application::ApplicationRunResult::NormalExit
		? EXIT_SUCCESS: EXIT_FAILURE;
}
