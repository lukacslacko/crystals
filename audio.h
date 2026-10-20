#pragma once
#include "sound_model.h"
#include <memory>
#include <string>

class GlassAudio {
public:
 GlassAudio();~GlassAudio();
 bool start(const wchar_t*deviceId=nullptr);void stop();void configure(const SoundDescription&description);void submit(const SoundFrame&frame);void setSettings(const AudioSettings&settings);
 bool available()const;uint64_t renderedFrames()const;uint64_t droppedFrames()const;std::string status()const;
private:
 struct Impl;std::unique_ptr<Impl>impl;
};
