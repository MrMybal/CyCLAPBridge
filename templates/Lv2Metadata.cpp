// SPDX-License-Identifier: MIT
#include <cyclapbridge/Plugin.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include "NativeConfig.h"
namespace {
std::string escape(const std::string& text){
    std::string out;for(unsigned char c:text){if(c=='"'||c=='\\')out+='\\';if(c=='\n'){out+="\\n";continue;}if(c=='\r')continue;out+=static_cast<char>(c);}return out;
}
int run(const std::filesystem::path& source,const std::filesystem::path& destination){
    try{
        cybridge::HostedPlugin plugin(cybridge::parseFormat(CYBRIDGE_SOURCE_FORMAT),source,CYBRIDGE_SOURCE_ID);
        std::filesystem::create_directories(destination);
        std::ofstream ttl(destination/"plugin.ttl"),header(destination/"Lv2Metadata.h");
        ttl.exceptions(std::ios::badbit|std::ios::failbit);header.exceptions(std::ios::badbit|std::ios::failbit);
        ttl<<std::setprecision(17)
           <<"@prefix lv2: <http://lv2plug.in/ns/lv2core#> .\n@prefix doap: <http://usefulinc.com/ns/doap#> .\n"
             "@prefix atom: <http://lv2plug.in/ns/ext/atom#> .\n@prefix midi: <http://lv2plug.in/ns/ext/midi#> .\n"
             "@prefix urid: <http://lv2plug.in/ns/ext/urid#> .\n@prefix state: <http://lv2plug.in/ns/ext/state#> .\n"
           <<"<"<<CYBRIDGE_PLUGIN_URI<<"> a lv2:Plugin ; doap:name \""<<escape(CYBRIDGE_PLUGIN_NAME)<<"\" ;\n"
             "lv2:requiredFeature urid:map ; lv2:extensionData state:interface ;\nlv2:port\n";
        std::uint32_t index=0,ni=0,no=0;
        auto audio=[&](const auto& ports,bool input){for(const auto& p:ports)for(std::uint32_t c=0;c<p.channels;++c){
            if(index)ttl<<",\n";ttl<<"[ a lv2:AudioPort, lv2:"<<(input?"InputPort":"OutputPort")<<" ; lv2:index "<<index
            <<" ; lv2:symbol \""<<(input?"in":"out")<<(input?ni:no)<<"\" ; lv2:name \""<<escape(p.name)<<" "<<c+1<<"\" ]";
            ++index;if(input)++ni;else ++no;
        }};
        audio(plugin.inputs(),true);audio(plugin.outputs(),false);
        for(const auto& p:plugin.parameters()){
            if(index)ttl<<",\n";ttl<<"[ a lv2:ControlPort, lv2:InputPort ; lv2:index "<<index++
            <<" ; lv2:symbol \"p"<<p.id<<"\" ; lv2:name \""<<escape(p.name)<<"\" ; lv2:minimum "<<p.minimum
            <<" ; lv2:maximum "<<p.maximum<<" ; lv2:default "<<p.defaultValue;
            if(p.stepped)ttl<<" ; lv2:portProperty lv2:integer";ttl<<" ]";
        }
        for(bool input:{true,false}){
            if(index)ttl<<",\n";ttl<<"[ a atom:AtomPort, lv2:"<<(input?"InputPort":"OutputPort")<<" ; lv2:index "<<index++
            <<" ; lv2:symbol \""<<(input?"midi_in":"midi_out")<<"\" ; lv2:name \"MIDI\" ; atom:bufferType atom:Sequence ; atom:supports midi:MidiEvent ]";
        }
        ttl<<" .\n";
        header<<"#pragma once\n#define CYBRIDGE_INPUT_CHANNELS "<<ni<<"\n#define CYBRIDGE_OUTPUT_CHANNELS "<<no
              <<"\n#define CYBRIDGE_PARAMETER_COUNT "<<plugin.parameters().size()<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){return argc==3?run(argv[1],argv[2]):2;}
#else
int main(int argc,char** argv){return argc==3?run(argv[1],argv[2]):2;}
#endif
