// SPDX-License-Identifier: MIT
#include <cyclapbridge/Conversion.h>
#include <cyclapbridge/Plugin.h>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
namespace {
thread_local bool auditing=false;
thread_local unsigned allocations=0,frees=0;
void check(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
template<class F> void rejects(F f,const char* message) {
    bool failed=false; try { f(); } catch(const std::exception&) { failed=true; } check(failed,message);
}
}
void* operator new(std::size_t n) { if(auditing) ++allocations; if(auto p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if(auditing && p) ++frees; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }
int main(int argc,char** argv) {
    try {
        check(argc==3,"Expected fixture and workspace paths");
        auto fixture=std::filesystem::absolute(argv[1]),work=std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(work);
        rejects([]{cybridge::parseFormat("aax");},"Removed AAX format was accepted");
        rejects([]{cybridge::parseFormat("AAX");},"Removed AAX format was accepted case-insensitively");
        for(const auto& capability:cybridge::conversionCapabilities())
            check(cybridge::formatName(capability.source)!="AAX" && cybridge::formatName(capability.target)!="AAX",
                  "Removed AAX format is advertised");
        cybridge::ClapPlugin plugin(fixture),second(fixture);
        check(plugin.descriptor().id=="org.cybridge.tests.gain","Descriptor ID");
        check(plugin.parameters().size()==1 && plugin.parameters()[0].id==7,"Stable parameter metadata");
        plugin.prepare(48000,64,16); check(plugin.start(),"Audio start");
        std::array<float,64> a,b,c,d;
        a.fill(0.25f); b.fill(-0.5f); c.fill(0); d.fill(0);
        float* in[]{a.data(),b.data()},*out[]{c.data(),d.data()};
        std::array<cybridge::AudioBus,1> inputs{{{in,2}}},outputs{{{out,2}}};
        std::array<cybridge::Event,1> events{};
        events[0].frame=17; events[0].parameterId=7; events[0].value=0.5;
        std::array<cybridge::Event,2> received{};
        cybridge::ProcessBlock block{64,inputs,outputs,events,received};
        auditing=true; auto result=plugin.process(block); auditing=false;
        check(result==cybridge::ProcessStatus::ok,"Audio processing");
        check(!allocations && !frees,"Prepared host processing allocated/freed");
        check(c[16]==0.25f && c[17]==0.125f && d[63]==-0.25f,"Sample-offset automation");
        check(block.outputEventCount==1 && received[0].frame==17,"Output event timestamp");
        events[0].frame=64; c.fill(123);
        check(plugin.process(block)==cybridge::ProcessStatus::invalidBlock && c[0]==123,"Invalid event touched outputs");
        events[0].frame=0; block.outputEvents={};
        check(plugin.process(block)==cybridge::ProcessStatus::ok && block.outputEventsDropped,"Output capacity not reported");
        rejects([&]{plugin.saveState();},"State while processing");
        plugin.stop(); auto state=plugin.saveState();
        check(state.size()==sizeof(double),"State serialization");
        rejects([&]{plugin.saveState(1);},"State capacity");
        plugin.loadState(state);
        rejects([&]{plugin.loadState({});},"Truncated state");
        plugin.release();
        rejects([&]{cybridge::ClapPlugin relative("relative.clap");},"Relative module load");
        cybridge::ConversionRequest request;
        request.input=fixture; request.projectDirectory=work/"generated project";
        request.outputName="CyBridge Test"; request.backend=cybridge::backendFromRoot(CYBRIDGE_TEST_ROOT);
        for(unsigned n=0;std::filesystem::exists(request.projectDirectory);++n)
            request.projectDirectory=work/("generated project "+std::to_string(n));
        auto project=cybridge::createConversionProject(request);
        check(std::filesystem::exists(project.directory/"PayloadEntry.cpp"),"Payload entry generation");
        check(std::filesystem::exists(project.directory/"payload"/fixture.filename()),"Payload copy");
        rejects([&]{cybridge::createConversionProject(request);},"Overwrite was accepted");
        request.projectDirectory=work/"unavailable"; request.target=cybridge::Format::jsfx;
        rejects([&]{cybridge::createConversionProject(request);},"Unsupported target was accepted");
        check(!std::filesystem::exists(request.projectDirectory),"Unsupported conversion wrote files");
        request.target=cybridge::Format::vst3; request.outputName="bad;name";
        rejects([&]{cybridge::createConversionProject(request);},"Unsafe CMake name accepted");
        std::cout<<"PASS: lifecycle, audio, automation, state/event bounds, allocation audit, conversion, overwrite protection, removed format rejection\n";
        return 0;
    } catch(const std::exception& e) { auditing=false; std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
