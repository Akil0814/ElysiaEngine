#include "game/demo/physics/physics_scenario.h"
#include "tests/support/test_assertions.h"
#include <cmath>
#include <iostream>
#include <set>

using namespace example::demo::physics;
using elysia::tests::require;
namespace
{
ScenarioResult run(const ScenarioDescriptor& d,int tier,int fps=60)
{
    PhysicsScenario scenario(d.id,tier);
    require(scenario.result().status==ScenarioStatus::Ready,"Cases enter ready without automatically starting");
    scenario.advance(1);
    require(scenario.result().steps==0,"Ready case does not simulate");
    scenario.start();scenario.set_paused(true);scenario.advance(1);
    require(scenario.result().steps==0,"Paused case does not simulate");
    scenario.single_step();
    require(scenario.result().steps==1&&scenario.paused(),"Single-step executes exactly one tick and stays paused");
    scenario.set_paused(false);
    for(unsigned frame=0;frame<d.max_steps*unsigned(fps)/60+unsigned(fps)*2 && scenario.result().status==ScenarioStatus::Running;++frame)
        scenario.advance(1.0/fps);
    const auto result=scenario.result();
    if(result.status!=ScenarioStatus::Passed)
    {
        std::cerr<<d.id<<" tier="<<tier<<" step="<<result.steps<<" failure="<<result.failure<<'\n';
        for(const auto& c:result.checks)if(!c.passed)std::cerr<<c.id<<": actual="<<c.actual<<" expected="<<c.expected<<" tolerance="<<c.tolerance<<'\n';
    }
    require(result.status==ScenarioStatus::Passed,"Scenario behavior checks pass");
    require(!result.checks.empty(),"Every catalog case performs real checks");
    if(d.category==ScenarioCategory::Stress){require(result.samples==600,"Pressure test samples all 600 measured steps");
        require(std::isfinite(result.max_ms)&&result.max_ms>=result.p95_ms&&result.p95_ms>=result.median_ms,"Timing percentiles are ordered");}
    std::vector<elysia::core::RenderCommand> commands;scenario.submit_render_commands(commands);
    if(d.id!="lifecycle")require(!commands.empty(),"Every live fixture is also visible");
    return result;
}
}
int main(int argc,char** argv)
{
    std::set<std::string_view> ids;
    for(const auto& d:physics_scenarios()){
        require(ids.insert(d.id).second,"Scenario IDs are unique");
        require(d.max_steps>0&&!d.title_key().empty()&&!d.purpose_key().empty()&&!d.expected_key().empty(),"Catalog metadata is complete");
        require(find_physics_scenario(d.id)==&d,"Every catalog ID resolves");
    }
    if(argc>1){
        auto* d=find_physics_scenario(argv[1]);require(d!=nullptr,"Requested case exists");
        const int tier=argc>2?std::stoi(argv[2]):0;
        auto first=run(*d,tier);
        if(d->category!=ScenarioCategory::Stress){
            auto repeat=run(*d,tier);
            require(first.checks.size()==repeat.checks.size()&&first.steps==repeat.steps,"Reconstruction preserves script and check count");
            for(std::size_t i=0;i<first.checks.size();++i){require(first.checks[i].id==repeat.checks[i].id&&first.checks[i].passed==repeat.checks[i].passed,"Reconstruction preserves outcomes");}
        }
        if(d->id=="timing")for(int fps:{30,120,144}){auto r=run(*d,tier,fps);require(r.steps==first.steps,"Display cadence preserves fixed tick count");}
        remember_scenario_result(d->id,tier,first);
        require(recent_scenario_result(d->id,tier)->status==ScenarioStatus::Passed,"Session history retains results");
        std::cout<<d->id<<" passed "<<first.checks.size()<<" checks in "<<first.steps<<" steps\n";
    }else{
        PhysicsScenario timeout("motion");timeout.start();timeout.finish_timeout();
        require(timeout.result().status==ScenarioStatus::Failed&&timeout.result().failure=="timeout","Timeout is a visible failure");
        PhysicsScenario debug("stress_bodies");debug.start();for(int i=0;i<122;++i)debug.advance(1.0/60);
        debug.set_debug_geometry(true);require(debug.result().mixed_debug_samples,"Changing debug mode labels mixed timing samples");
        bool rejected=false;try{PhysicsScenario invalid("missing");}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"Unknown IDs are rejected");
    }
}
