#include "velocity_bridge.hpp"

namespace cas_velocity
{
    namespace
    {
        context g_context{};
    }

    context& global_context()
    {
        return g_context;
    }

    void install_adapter(const adapter& game_adapter)
    {
        g_context.game = game_adapter;
        if (!g_context.initialized)
            initialize(g_context);
    }
}

extern "C" __declspec(dllexport) cas_velocity::context* CasVelocityGetContext()
{
    return &cas_velocity::global_context();
}

extern "C" __declspec(dllexport) cas_velocity::settings* CasVelocityGetSettings()
{
    return &cas_velocity::global_context().config;
}

extern "C" __declspec(dllexport) void CasVelocityInstallAdapter(const cas_velocity::adapter* game_adapter)
{
    if (!game_adapter)
        return;
    cas_velocity::install_adapter(*game_adapter);
}

extern "C" __declspec(dllexport) void CasVelocityResetRuntime()
{
    cas_velocity::reset_runtime(cas_velocity::global_context());
}

extern "C" __declspec(dllexport) void CasVelocityDispatchCreateMove(cas_velocity::command* cmd)
{
    if (!cmd)
        return;
    cas_velocity::on_create_move(cas_velocity::global_context(), *cmd);
}

extern "C" __declspec(dllexport) void CasVelocityDispatchFrameStagePre(int stage)
{
    cas_velocity::on_frame_stage(cas_velocity::global_context(), stage, false);
}

extern "C" __declspec(dllexport) void CasVelocityDispatchFrameStagePost(int stage)
{
    cas_velocity::on_frame_stage(cas_velocity::global_context(), stage, true);
}
