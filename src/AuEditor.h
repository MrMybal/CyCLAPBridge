// SPDX-License-Identifier: MIT
#pragma once
#include <cyclapbridge/Editor.h>
#include <cyclapbridge/Plugin.h>
#include <AudioToolbox/AudioToolbox.h>
namespace cybridge::detail {
class AuEditor {
public:
    virtual ~AuEditor()=default;
    virtual EditorSize size() const=0;
    virtual bool canResize() const=0;
    virtual bool resize(EditorSize)=0;
    virtual bool parent(void*)=0;
    virtual bool show(bool)=0;
    virtual void parameter(std::uint32_t)=0;
};
bool hasAuEditor(AudioUnit);
std::unique_ptr<AuEditor> makeAuEditor(AudioUnit,const std::vector<Parameter>&,EditorCallbacks);
}
