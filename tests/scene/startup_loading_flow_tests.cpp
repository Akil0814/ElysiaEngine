#include "engine/builtin/scenes/startup_loading_flow.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>

namespace
{
using elysia::builtin::StartupLoadingAction;
using elysia::builtin::StartupLoadingCompletion;
using elysia::builtin::StartupLogoAction;
using elysia::builtin::StartupLogoSequence;
using elysia::tests::require;

void test_engine_logo_always_precedes_optional_project_logo()
{
    StartupLogoSequence engine_only;
    engine_only.reset(false);
    require(engine_only.start() == StartupLogoAction::PlayEngineLogo,
        "the engine logo must always be the first startup logo");
    require(engine_only.engine_logo_finished()
        == StartupLogoAction::IntroFinished,
        "an engine-only startup must finish after the Elysia logo");

    StartupLogoSequence branded;
    branded.reset(true);
    require(branded.start() == StartupLogoAction::PlayEngineLogo,
        "project branding must not replace or precede the engine logo");
    require(branded.engine_logo_finished()
        == StartupLogoAction::PlayProjectLogo,
        "the optional project logo must start after the engine logo");
    require(branded.project_logo_finished()
        == StartupLogoAction::IntroFinished,
        "the branded intro must finish after the project logo");
    require(branded.engine_logo_finished() == StartupLogoAction::None,
        "completed logo callbacks must be idempotent");
}

void test_loading_and_intro_can_finish_in_either_order()
{
    StartupLoadingCompletion loading_first;
    loading_first.reset(false,true);
    require(loading_first.mark_loading_finished()
        == StartupLoadingAction::None,
        "content completion alone must not leave startup");
    require(loading_first.mark_intro_finished()
        == StartupLoadingAction::TransitionToSuccess,
        "automatic startup must transition after intro catches up");

    StartupLoadingCompletion intro_first;
    intro_first.reset(false,true);
    require(intro_first.mark_intro_finished()
        == StartupLoadingAction::None,
        "intro completion alone must not leave startup");
    require(intro_first.mark_loading_finished()
        == StartupLoadingAction::TransitionToSuccess,
        "automatic startup must transition after loading catches up");
}

void test_logo_sequence_can_be_skipped_after_loading()
{
    StartupLoadingCompletion loading_first;
    loading_first.reset(false,false);
    require(loading_first.mark_loading_finished()
        == StartupLoadingAction::TransitionToSuccess,
        "fast startup must transition as soon as content finishes");
    require(loading_first.mark_intro_finished() == StartupLoadingAction::None,
        "a later logo callback must not queue a second transition");
    require(loading_first.fail() == StartupLoadingAction::None,
        "a failure callback after fast success must not replace the transition");

    StartupLoadingCompletion intro_first;
    intro_first.reset(false,false);
    require(intro_first.mark_intro_finished() == StartupLoadingAction::None,
        "fast startup must still wait for content");
    require(intro_first.mark_loading_finished()
        == StartupLoadingAction::TransitionToSuccess,
        "fast startup must transition when content catches up");
}

void test_fast_startup_can_still_require_confirmation()
{
    StartupLoadingCompletion confirmation;
    confirmation.reset(true,false);
    require(confirmation.mark_loading_finished()
        == StartupLoadingAction::WaitForConfirmation,
        "fast startup must expose confirmation without waiting for logos");
    require(confirmation.waiting_for_confirmation(),
        "fast startup must report that confirmation is pending");
    require(confirmation.mark_intro_finished() == StartupLoadingAction::None,
        "background logo completion must not dismiss confirmation");
    require(confirmation.confirm()
        == StartupLoadingAction::TransitionToSuccess,
        "confirmation must complete fast startup");
    require(confirmation.confirm() == StartupLoadingAction::None,
        "repeated fast-startup confirmation must not queue another transition");
    require(confirmation.fail() == StartupLoadingAction::None,
        "a failure callback after confirmed fast startup must be ignored");
}

void test_confirmation_and_failure_are_single_terminal_actions()
{
    StartupLoadingCompletion confirmation;
    confirmation.reset(true,true);
    require(confirmation.mark_intro_finished()
        == StartupLoadingAction::None,
        "confirmation mode must still wait for content");
    require(confirmation.mark_loading_finished()
        == StartupLoadingAction::WaitForConfirmation,
        "confirmation mode must expose the prompt only after both branches finish");
    require(confirmation.waiting_for_confirmation(),
        "completion state must report that confirmation is pending");
    require(confirmation.confirm()
        == StartupLoadingAction::TransitionToSuccess,
        "confirmation input must produce the success transition");
    require(confirmation.confirm() == StartupLoadingAction::None,
        "repeated confirmation input must not queue a second route");

    StartupLoadingCompletion failure;
    failure.reset(true,true);
    require(failure.fail() == StartupLoadingAction::TransitionToFailure,
        "a startup failure must produce the failure transition");
    require(failure.fail() == StartupLoadingAction::None
        && failure.mark_loading_finished() == StartupLoadingAction::None
        && failure.mark_intro_finished() == StartupLoadingAction::None,
        "failure must suppress all later completion actions");
}
}

int main()
{
    test_engine_logo_always_precedes_optional_project_logo();
    test_loading_and_intro_can_finish_in_either_order();
    test_logo_sequence_can_be_skipped_after_loading();
    test_fast_startup_can_still_require_confirmation();
    test_confirmation_and_failure_are_single_terminal_actions();
    return EXIT_SUCCESS;
}
