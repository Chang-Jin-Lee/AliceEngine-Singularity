// SPDX-License-Identifier: MIT
// Equivalent query modes isolate the benefit of indexing from changes in collision behavior.
#include "Runtime/Physics/Platformer.h"
#include "Foundation/Profiler.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>
using namespace alice;
using namespace alice::runtime::physics;
using Clock = std::chrono::steady_clock;
int main() {
    Profiler::SetEnabled(false);
    constexpr int kWarmup = 20, kSamples = 100;
    std::cout << "{\"warmup\":20,\"samples\":100,\"profiler_enabled\":false,\"fixed_dt\":0.016666666666666666,\"rows\":[";
    bool first = true;
    for (const int count : {100,1000,10000}) {
        for (const bool dense : {false,true}) {
            std::vector<Box> terrain;
            for (int i=0;i<count;++i) {
                const double spacing = dense ? 1.0 : 8.0;
                terrain.push_back({{static_cast<double>(i%100)*spacing,
                                    -static_cast<double>(i/100)*spacing-0.5},{0.45,0.5}});
            }
            for (const int actors : {1,100}) {
                StaticWorld reference(false);
                if (!reference.Build(terrain)) return 2;
                for (const bool indexed : {false,true}) {
                    StaticWorld world(indexed);
                    const auto buildStart = Clock::now();
                    if (!world.Build(terrain)) return 2;
                    const double buildUs = std::chrono::duration<double,std::micro>(Clock::now()-buildStart).count();
                    std::vector<double> durations;
                    usize candidates=0,tests=0;
                    for (int sample=-kWarmup;sample<kSamples;++sample) {
                        const auto start = Clock::now();
                        for (int actor=0;actor<actors;++actor) {
                            const Box box{{static_cast<double>(actor)*(dense?1.0:8.0),0.8},{0.25,0.5}};
                            auto motion = world.Move(box,{0.03,-0.4});
                            if (!motion) return 3;
                            if (sample>=0) { candidates+=motion->candidates; tests+=motion->tests; }
                        }
                        const double us = std::chrono::duration<double,std::micro>(Clock::now()-start).count();
                        if(sample>=0) durations.push_back(us);
                    }
                    // Correctness comparisons are deliberately outside the timed region.
                    for(int actor=0;actor<actors;++actor) {
                        Box box{{static_cast<double>(actor)*(dense?1.0:8.0),0.8},{0.25,0.5}};
                        auto actual=world.Move(box,{0.03,-0.4});
                        auto expected=reference.Move(box,{0.03,-0.4});
                        if(!actual || !expected || actual->box.center.x!=expected->box.center.x ||
                           actual->box.center.y!=expected->box.center.y || actual->grounded!=expected->grounded) return 4;
                    }
                    double mean=0; for(double us:durations) mean+=us;
                    mean/=static_cast<double>(durations.size());
                    std::sort(durations.begin(),durations.end());
                    if(!first) std::cout << ',';
                    first=false;
                    std::cout << "{\"terrain\":"<<count<<",\"actors\":"<<actors
                              <<",\"dense\":"<<(dense?"true":"false")<<",\"indexed\":"<<(indexed?"true":"false")
                              <<",\"build_us\":"<<buildUs<<",\"step_mean_us\":"<<mean
                              <<",\"step_p95_us\":"<<durations[94]<<",\"candidates\":"<<candidates
                              <<",\"exact_tests\":"<<tests<<'}';
                }
            }
        }
    }
    std::cout << "]}\n";
}
