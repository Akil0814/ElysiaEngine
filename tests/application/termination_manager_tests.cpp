#include "engine/application/lifecycle/application_event_boundary.h"
#include "engine/application/lifecycle/application_exit_policy.h"
#include "engine/tools/termination_manager.h"
#include "tests/support/test_assertions.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
using elysia::tests::require;

void test_lifecycle_and_first_request_wins()
{
    using namespace elysia::tools;
    auto* manager = TerminationManager::instance();
    manager->reset_for_testing();
    require(!manager->termination_requested(),"reset manager must not report a termination request");
    require(!manager->termination_info().has_value(),"reset manager must not expose termination information");

    const unsigned int request_line = __LINE__ + 1;
    manager->request_termination(TerminationReason::FatalRuntimeFailure,"resource","first failure");
    const auto info = manager->termination_info();
    require(info.has_value(),"published request must expose termination information");
    require(info->reason == TerminationReason::FatalRuntimeFailure,"request must preserve its reason");
    require(info->category == "resource" && info->message == "first failure",
        "request must preserve diagnostic text");
    require(info->location.line() == request_line,"request must preserve its call site");

    manager->request_termination(TerminationReason::UnhandledException,"input","second failure");
    const auto first_info = manager->termination_info();
    require(first_info->reason == TerminationReason::FatalRuntimeFailure
            && first_info->message == "first failure",
        "later termination requests must not overwrite the root cause");
}

void test_truncation_and_shutdown_sealing()
{
    using namespace elysia::tools;
    auto* manager = TerminationManager::instance();
    manager->reset_for_testing();
    const std::string long_message(2048,'x');
    manager->request_termination(TerminationReason::FatalRuntimeFailure,"resource",long_message);
    const auto truncated_info = manager->termination_info();
    require(truncated_info.has_value() && truncated_info->message_truncated,
        "fixed-size termination diagnostics must report truncation");
    require(truncated_info->message.size() < long_message.size(),
        "truncated termination messages must fit the fixed record");

    manager->reset_for_testing();
    manager->request_termination(TerminationReason::FatalRuntimeFailure,"worker","before seal");
    require(manager->seal_for_shutdown(),"a published termination request must win while sealing");

    manager->reset_for_testing();
    require(!manager->seal_for_shutdown(),"clean shutdown sealing must report no fault");
    manager->request_termination(TerminationReason::FatalRuntimeFailure,"worker","after seal");
    require(!manager->termination_requested(),"sealed manager must reject later requests");
}

void test_worker_publication_is_complete()
{
    using namespace elysia::tools;
    auto* manager = TerminationManager::instance();
    for (int iteration = 0; iteration < 100; ++iteration)
    {
        manager->reset_for_testing();
        std::atomic<bool> worker_finished = false;
        std::thread worker([manager,&worker_finished]()
        {
            manager->request_termination(
                TerminationReason::FatalRuntimeFailure,"worker","complete published diagnostic");
            worker_finished.store(true,std::memory_order_release);
        });

        while (!worker_finished.load(std::memory_order_acquire) && !manager->termination_requested())
        {
            if (const auto pending_info = manager->termination_info())
            {
                require(pending_info->reason == TerminationReason::FatalRuntimeFailure
                        && pending_info->category == "worker"
                        && pending_info->message == "complete published diagnostic",
                    "observers must never receive a partial termination record");
            }
        }
        worker.join();
        const auto info = manager->termination_info();
        require(info.has_value() && info->category == "worker"
                && info->message == "complete published diagnostic",
            "worker requests must be safely visible to the main thread");
    }
}

void test_event_boundary_and_exit_priority()
{
    using namespace elysia;
    auto* manager = tools::TerminationManager::instance();

    manager->reset_for_testing();
    const bool standard_result = elysia::application::run_event_boundary("input",[]()
    {
        throw std::runtime_error("standard boundary exception");
    });
    require(!standard_result,"standard exceptions must fail the event boundary");
    const auto standard_info = manager->termination_info();
    require(standard_info.has_value()
            && standard_info->reason == tools::TerminationReason::UnhandledException
            && standard_info->category == "input"
            && standard_info->message == "standard boundary exception",
        "event boundaries must publish standard exception diagnostics");

    manager->reset_for_testing();
    const bool unknown_result = elysia::application::run_event_boundary("render",[]()
    {
        throw 7;
    });
    require(!unknown_result,"unknown exceptions must fail the event boundary");
    const auto unknown_info = manager->termination_info();
    require(unknown_info.has_value()
            && unknown_info->reason == tools::TerminationReason::UnhandledException
            && unknown_info->category == "render",
        "event boundaries must publish unknown exception diagnostics");

    manager->reset_for_testing();
    require(elysia::application::resolve_application_exit(false,*manager)
            == elysia::application::ApplicationExitDecision::Continue,
        "no request and no normal quit must continue");
    const auto normal_exit =
        elysia::application::resolve_application_exit(true,*manager);
    require(normal_exit == elysia::application::ApplicationExitDecision::NormalExit,
        "normal quit without a fault must exit successfully");
    require(
        elysia::application::to_application_run_result(normal_exit)
            == elysia::application::ApplicationRunResult::NormalExit,
        "normal exit decisions must produce a normal application run result");

    manager->reset_for_testing();
    manager->request_termination(tools::TerminationReason::FatalRuntimeFailure,"worker","same frame fault");
    const auto fault_exit =
        elysia::application::resolve_application_exit(true,*manager);
    require(fault_exit == elysia::application::ApplicationExitDecision::FaultExit,
        "published termination requests must override normal quit requests");
    require(
        elysia::application::to_application_run_result(fault_exit)
            == elysia::application::ApplicationRunResult::FaultExit,
        "fault exit decisions must produce a fault application run result");
}
}

int main()
{
    test_lifecycle_and_first_request_wins();
    test_truncation_and_shutdown_sealing();
    test_worker_publication_is_complete();
    test_event_boundary_and_exit_priority();
    std::cout << "termination manager tests passed\n";
    return EXIT_SUCCESS;
}
