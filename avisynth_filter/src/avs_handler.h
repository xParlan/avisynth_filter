// License: https://github.com/CrendKing/avisynth_filter/blob/master/LICENSE

#pragma once

#include "environment.h"
#include "format.h"
#include "frame_handler.h"
#include "rc_ptr.h"
#include "source_clip.h"


namespace AvsFilter {

class AvsHandler {
private:
    class ScriptInstance {
    public:
        auto Initialize() const -> void;
        auto StopScript() -> void;

    protected:
        explicit ScriptInstance(AvsHandler &handler);
        virtual ~ScriptInstance();

        DISABLE_COPYING(ScriptInstance)

        virtual auto ReloadScript(const AM_MEDIA_TYPE &mediaType, bool ignoreDisconnect) -> bool;

        AvsHandler &_handler;
        IScriptEnvironment *_env;
        PClip _scriptClip = nullptr;
        VideoInfo _scriptVideoInfo = {};
        REFERENCE_TIME _scriptAvgFrameDuration = 0;
        std::string _errorString;
    };

public:
    class MainScriptInstance : public ScriptInstance {
    public:
        explicit MainScriptInstance(AvsHandler &handler);
        ~MainScriptInstance() override;

        DISABLE_COPYING(MainScriptInstance)

        auto ReloadScript(const AM_MEDIA_TYPE &mediaType, bool ignoreDisconnect) -> bool override;
        auto GetFrame(int frameNb) const -> PVideoFrame;
        constexpr auto GetEnv() const -> IScriptEnvironment * { return _env; }
        constexpr auto GetSourceDrainFrame() const -> const PVideoFrame & { return _sourceDrainFrame; }
        constexpr auto GetSourceAvgFrameDuration() const -> REFERENCE_TIME { return _sourceAvgFrameDuration; }
        constexpr auto GetSourceAvgFrameRate() const -> int { return _sourceAvgFrameRate; }
        constexpr auto GetScriptAvgFrameDuration() const -> REFERENCE_TIME { return _scriptAvgFrameDuration; }
        auto GetErrorString() const -> std::optional<std::string>;

    private:
        PVideoFrame _sourceDrainFrame = nullptr;
        REFERENCE_TIME _sourceAvgFrameDuration = 0;
        int _sourceAvgFrameRate = 0;
    };

    class CheckingScriptInstance : public ScriptInstance {
    public:
        explicit CheckingScriptInstance(AvsHandler &handler);

        DISABLE_COPYING(CheckingScriptInstance)

        auto ReloadScript(const AM_MEDIA_TYPE &mediaType, bool ignoreDisconnect) -> bool override;
        auto GenerateMediaType(const Format::PixelFormat &pixelFormat, const AM_MEDIA_TYPE *templateMediaType) const -> CMediaType;
        constexpr auto GetScriptPixelType() const -> int { return _scriptVideoInfo.pixel_type; }
    };

    AvsHandler();
    ~AvsHandler();

    DISABLE_COPYING(AvsHandler)

    auto LinkFrameHandler(FrameHandler &frameHandler) const -> void;
    auto SetScriptPath(const std::filesystem::path &scriptPath) -> void;
    constexpr auto GetVersionString() const -> const char * { return _versionString == nullptr ? "unknown AviSynth version" : _versionString; }
    constexpr auto GetScriptPath() const -> const std::filesystem::path & { return _scriptPath; }
    auto GetMainScriptInstance() const -> MainScriptInstance &;
    auto GetCheckingScriptInstance() const -> CheckingScriptInstance &;

private:
    auto LoadAvsModule() const -> HMODULE;
    auto CreateEnv() const -> IScriptEnvironment *;
    [[ noreturn ]] auto ShowFatalError(const WCHAR *errorMessage) const -> void;
    auto GetSourceClip() const -> SourceClip *;

    HMODULE _avsModule = LoadAvsModule();

    std::unique_ptr<MainScriptInstance> _mainScriptInstance = std::make_unique<MainScriptInstance>(*this);
    std::unique_ptr<CheckingScriptInstance> _checkingScriptInstance = std::make_unique<CheckingScriptInstance>(*this);
    const char *_versionString = _mainScriptInstance->GetEnv()->Invoke("Eval", AVSValue("VersionString()")).AsString();

    std::filesystem::path _scriptPath = g_env.GetAvsPath();
    VideoInfo _sourceVideoInfo = {};
    PClip _sourceClip = new SourceClip(_sourceVideoInfo);
};

extern ReferenceCountPointer<AvsHandler> g_avs;

}
