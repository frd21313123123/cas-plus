#pragma once

#include "velocity_port.hpp"

namespace cas_velocity
{
    context& global_context();
    void install_adapter(const adapter& game_adapter);
}

extern "C"
{
    __declspec(dllexport) cas_velocity::context* CasVelocityGetContext();
    __declspec(dllexport) cas_velocity::settings* CasVelocityGetSettings();
    __declspec(dllexport) void CasVelocityInstallAdapter(const cas_velocity::adapter* game_adapter);
    __declspec(dllexport) void CasVelocityResetRuntime();
    __declspec(dllexport) void CasVelocityDispatchCreateMove(cas_velocity::command* cmd);
    __declspec(dllexport) void CasVelocityDispatchFrameStagePre(int stage);
    __declspec(dllexport) void CasVelocityDispatchFrameStagePost(int stage);
}
