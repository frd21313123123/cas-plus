#include "velocity_port.hpp"

namespace cas_velocity
{
    namespace
    {
        constexpr float k_pi = 3.14159265358979323846f;
        constexpr float k_deg_to_rad = k_pi / 180.0f;
        constexpr float k_rad_to_deg = 180.0f / k_pi;

        float absf(float v)
        {
            return v < 0.0f ? -v : v;
        }

        float minf(float a, float b)
        {
            return a < b ? a : b;
        }

        float maxf(float a, float b)
        {
            return a > b ? a : b;
        }

        float clampf(float value, float low, float high)
        {
            if (value < low)
                return low;
            if (value > high)
                return high;
            return value;
        }

        float wrap_radians(float value)
        {
            while (value > k_pi)
                value -= 2.0f * k_pi;
            while (value < -k_pi)
                value += 2.0f * k_pi;
            return value;
        }

        float fast_sin(float radians)
        {
            const float x = wrap_radians(radians);
            const float x2 = x * x;
            return x * (1.0f - x2 / 6.0f + (x2 * x2) / 120.0f - (x2 * x2 * x2) / 5040.0f);
        }

        float fast_cos(float radians)
        {
            const float x = wrap_radians(radians);
            const float x2 = x * x;
            return 1.0f - x2 / 2.0f + (x2 * x2) / 24.0f - (x2 * x2 * x2) / 720.0f;
        }

        float length2d_sqr(const vec3& value)
        {
            return value.x * value.x + value.y * value.y;
        }

        rage_weapon_settings& rage_group(settings& config, weapon_group group)
        {
            unsigned int index = static_cast<unsigned int>(group);
            const unsigned int count = static_cast<unsigned int>(weapon_group::count);
            if (index >= count)
                index = static_cast<unsigned int>(weapon_group::rifle);
            return config.rage.groups[index];
        }

        legit_weapon_settings& legit_group(settings& config, weapon_group group)
        {
            unsigned int index = static_cast<unsigned int>(group);
            const unsigned int count = static_cast<unsigned int>(weapon_group::count);
            if (index >= count)
                index = static_cast<unsigned int>(weapon_group::rifle);
            return config.legit.groups[index];
        }

        bool should_skip_antiaim(const player_state& local, const command& cmd)
        {
            if (!local.valid || !local.alive || local.on_ladder)
                return true;
            if ((cmd.buttons & button_use) != 0u)
                return true;
            if ((cmd.buttons & button_attack) != 0u && local.can_fire)
                return true;
            return false;
        }

        int select_target(const target_state* targets, unsigned int count, autoyaw_mode mode)
        {
            int selected = -1;
            float best_float = 0.0f;
            int best_health = 0;

            for (unsigned int i = 0; i < count; ++i)
            {
                const target_state& target = targets[i];
                if (!target.valid || !target.alive)
                    continue;

                if (selected < 0)
                {
                    selected = static_cast<int>(i);
                    best_float = mode == autoyaw_mode::distance ? target.distance_sqr : target.crosshair_delta_sqr;
                    best_health = target.health;
                    continue;
                }

                if (mode == autoyaw_mode::health)
                {
                    if (target.health < best_health)
                    {
                        selected = static_cast<int>(i);
                        best_health = target.health;
                    }
                }
                else
                {
                    const float metric = mode == autoyaw_mode::distance ? target.distance_sqr : target.crosshair_delta_sqr;
                    if (metric < best_float)
                    {
                        selected = static_cast<int>(i);
                        best_float = metric;
                    }
                }
            }
            return selected;
        }

        void apply_bunnyhop(const movement_settings& config, const player_state& local, command& cmd)
        {
            if (!config.bunnyhop || local.on_ladder)
                return;
            if ((cmd.buttons & button_jump) != 0u && !local.on_ground)
                cmd.buttons &= ~button_jump;
        }

        void apply_slowwalk(const movement_settings& config, const player_state& local, command& cmd)
        {
            if (!config.slow_walk || !local.on_ground)
                return;

            const float requested_sqr = cmd.forward_move * cmd.forward_move + cmd.side_move * cmd.side_move;
            const float limit = maxf(config.slow_walk_speed, 1.0f);
            const float limit_sqr = limit * limit;
            if (requested_sqr <= limit_sqr || requested_sqr <= 0.001f)
                return;

            float length = requested_sqr;
            float estimate = length > 1.0f ? length : 1.0f;
            for (int i = 0; i < 7; ++i)
                estimate = 0.5f * (estimate + length / estimate);

            if (estimate <= 0.001f)
                return;
            const float scale = limit / estimate;
            cmd.forward_move *= scale;
            cmd.side_move *= scale;
        }

        void apply_air_strafe(const movement_settings& config, const player_state& local, command& cmd)
        {
            if (!config.air_strafe || local.on_ground || local.on_ladder)
                return;

            const float horizontal = length2d_sqr(local.velocity);
            if (horizontal < 4.0f)
                return;

            if (absf(cmd.side_move) < 1.0f)
                cmd.side_move = (cmd.command_number & 1) != 0 ? 450.0f : -450.0f;
            cmd.forward_move = 0.0f;
        }

        void apply_edge_features(context& ctx, const player_state& local, command& cmd)
        {
            const movement_settings& config = ctx.config.movement;

            if (config.edge_jump && ctx.runtime.previous_on_ground && !local.on_ground)
                cmd.buttons |= button_jump;

            if (config.jump_bug && ctx.runtime.previous_on_ground && !local.on_ground)
            {
                cmd.buttons &= ~button_jump;
                cmd.buttons |= button_duck;
            }

            if (config.edge_stop && local.on_ground)
            {
                const float speed_sqr = length2d_sqr(local.velocity);
                if (speed_sqr > 2500.0f)
                {
                    cmd.forward_move = -local.velocity.x;
                    cmd.side_move = -local.velocity.y;
                }
            }

            if (config.edge_bug && !local.on_ground && local.velocity.z < -80.0f)
            {
                cmd.buttons |= button_duck;
                cmd.buttons &= ~button_jump;
            }
        }

        void apply_quickpeek(context& ctx, const player_state& local, command& cmd)
        {
            if (!ctx.config.assistance.quick_peek)
            {
                ctx.runtime.quick_peek_has_origin = false;
                ctx.runtime.quick_peek_retracking = false;
                return;
            }

            if (!ctx.runtime.quick_peek_has_origin && local.on_ground)
            {
                ctx.runtime.quick_peek_origin = local.origin;
                ctx.runtime.quick_peek_has_origin = true;
            }

            if ((cmd.buttons & button_attack) != 0u && local.can_fire)
                ctx.runtime.quick_peek_retracking = true;

            if (!ctx.runtime.quick_peek_retracking || !ctx.runtime.quick_peek_has_origin)
                return;

            const float dx = ctx.runtime.quick_peek_origin.x - local.origin.x;
            const float dy = ctx.runtime.quick_peek_origin.y - local.origin.y;
            const float dist_sqr = dx * dx + dy * dy;
            if (dist_sqr < 16.0f)
            {
                ctx.runtime.quick_peek_retracking = false;
                cmd.forward_move = 0.0f;
                cmd.side_move = 0.0f;
                return;
            }

            const float yaw = cmd.yaw * k_deg_to_rad;
            const float forward_x = fast_cos(yaw);
            const float forward_y = fast_sin(yaw);
            const float right_x = -forward_y;
            const float right_y = forward_x;

            cmd.forward_move = clampf(dx * forward_x + dy * forward_y, -450.0f, 450.0f);
            cmd.side_move = clampf(dx * right_x + dy * right_y, -450.0f, 450.0f);
        }

        bool apply_rage(context& ctx, const player_state& local, command& cmd, target_state* targets, unsigned int count)
        {
            if (!ctx.config.rage.enabled || !local.can_fire || count == 0u)
                return false;

            rage_weapon_settings& group = rage_group(ctx.config, local.weapon);
            int selected = -1;
            float best_delta = group.max_fov * group.max_fov;
            const int minimum_damage = group.min_damage_override ? group.min_damage_override_value : group.min_damage;
            const int minimum_hitchance = group.hitchance_override ? group.hitchance_override_value : group.hitchance;

            for (unsigned int i = 0; i < count; ++i)
            {
                const target_state& target = targets[i];
                if (!target.valid || !target.alive)
                    continue;
                if (target.crosshair_delta_sqr > best_delta)
                    continue;
                if (target.estimated_hitchance < minimum_hitchance)
                    continue;
                if (target.estimated_damage < minimum_damage && target.estimated_damage < target.health)
                    continue;
                if (target.backtrack_ticks > ctx.config.rage.max_backtrack_ticks)
                    continue;

                selected = static_cast<int>(i);
                best_delta = target.crosshair_delta_sqr;
            }

            if (selected < 0)
                return false;

            const float old_yaw = cmd.yaw;
            cmd.pitch = clamp_pitch(targets[selected].aim_pitch);
            cmd.yaw = normalize_yaw(targets[selected].aim_yaw);
            correct_movement(cmd, old_yaw, cmd.yaw);
            cmd.buttons |= button_attack;

            if (group.doubletap)
            {
                if (ctx.runtime.doubletap_charge < 14)
                    ++ctx.runtime.doubletap_charge;
                else
                    ctx.runtime.doubletap_charge = 0;
            }
            return true;
        }

        void apply_legit(context& ctx, const player_state& local, command& cmd, target_state* targets, unsigned int count)
        {
            if (!ctx.config.legit.enabled || count == 0u)
                return;

            legit_weapon_settings& group = legit_group(ctx.config, local.weapon);
            int selected = -1;
            float best = group.fov * group.fov;

            for (unsigned int i = 0; i < count; ++i)
            {
                const target_state& target = targets[i];
                if (!target.valid || !target.alive)
                    continue;
                if (!group.autowall && !target.visible)
                    continue;
                if (target.estimated_damage < group.min_damage && target.estimated_damage < target.health)
                    continue;
                if (target.crosshair_delta_sqr < best)
                {
                    selected = static_cast<int>(i);
                    best = target.crosshair_delta_sqr;
                }
            }

            if (selected < 0)
                return;

            const target_state& target = targets[selected];
            if (group.aimbot)
            {
                const int smooth = group.smooth < 1 ? 1 : group.smooth;
                const float old_yaw = cmd.yaw;
                const float yaw_delta = normalize_yaw(target.aim_yaw - cmd.yaw);
                const float pitch_delta = clamp_pitch(target.aim_pitch - cmd.pitch);
                cmd.yaw = normalize_yaw(cmd.yaw + yaw_delta / static_cast<float>(smooth));
                cmd.pitch = clamp_pitch(cmd.pitch + pitch_delta / static_cast<float>(smooth));
                correct_movement(cmd, old_yaw, cmd.yaw);
            }

            if (group.triggerbot && target.estimated_hitchance >= group.trigger_hitchance && local.can_fire)
            {
                ++ctx.runtime.trigger_hold_ticks;
                const int required_ticks = group.trigger_delay_ms <= 0 ? 0 : (group.trigger_delay_ms + 15) / 16;
                if (ctx.runtime.trigger_hold_ticks >= required_ticks)
                    cmd.buttons |= button_attack;
            }
            else
            {
                ctx.runtime.trigger_hold_ticks = 0;
            }
        }

        void apply_autos(context& ctx, const player_state& local, command& cmd)
        {
            if (ctx.config.assistance.auto_scope && local.weapon_is_sniper && !local.scoped && (cmd.buttons & button_attack) != 0u)
            {
                cmd.buttons &= ~button_attack;
                cmd.buttons |= button_attack2;
                if (ctx.game.set_scope)
                    ctx.game.set_scope(true);
            }

            if (ctx.config.assistance.auto_revolver && local.weapon_is_revolver && local.can_fire)
                cmd.buttons |= button_attack;
        }

        void apply_antiaim(context& ctx, const player_state& local, command& cmd, target_state* targets, unsigned int count)
        {
            antiaim_settings& config = ctx.config.antiaim;
            if (!config.enabled || should_skip_antiaim(local, cmd))
                return;

            if (config.pitch == pitch_mode::down)
                cmd.pitch = 89.0f;
            else if (config.pitch == pitch_mode::up)
                cmd.pitch = -89.0f;

            float base_yaw = local.view_yaw + 180.0f;
            if (config.manual == manual_direction::left)
                base_yaw = local.view_yaw + 90.0f;
            else if (config.manual == manual_direction::right)
                base_yaw = local.view_yaw - 90.0f;
            else if ((config.at_targets || config.autoyaw != autoyaw_mode::none) && count > 0u)
            {
                autoyaw_mode mode = config.autoyaw;
                if (mode == autoyaw_mode::none)
                    mode = autoyaw_mode::crosshair;
                const int selected = select_target(targets, count, mode);
                if (selected >= 0)
                    base_yaw = targets[selected].aim_yaw + 180.0f;
            }

            const float old_yaw = cmd.yaw;
            cmd.yaw = normalize_yaw(base_yaw);
            correct_movement(cmd, old_yaw, cmd.yaw);
            ctx.runtime.last_real_yaw = cmd.yaw;
            ctx.runtime.last_fake_yaw = normalize_yaw(cmd.yaw + 180.0f);
        }
    }

    void initialize(context& ctx)
    {
        reset_runtime(ctx);
        ctx.initialized = true;
    }

    void reset_runtime(context& ctx)
    {
        ctx.runtime = runtime_state{};
    }

    float normalize_yaw(float yaw)
    {
        while (yaw > 180.0f)
            yaw -= 360.0f;
        while (yaw < -180.0f)
            yaw += 360.0f;
        return yaw;
    }

    float clamp_pitch(float pitch)
    {
        return clampf(pitch, -89.0f, 89.0f);
    }

    vec3 extrapolate(const vec3& origin, const vec3& velocity, float interval, int ticks)
    {
        if (ticks < 0)
            ticks = 0;
        const float time = interval * static_cast<float>(ticks);
        return vec3{
            origin.x + velocity.x * time,
            origin.y + velocity.y * time,
            origin.z + velocity.z * time
        };
    }

    void correct_movement(command& cmd, float old_yaw, float new_yaw)
    {
        const float delta = normalize_yaw(new_yaw - old_yaw) * k_deg_to_rad;
        const float c = fast_cos(delta);
        const float s = fast_sin(delta);
        const float old_forward = cmd.forward_move;
        const float old_side = cmd.side_move;
        cmd.forward_move = clampf(c * old_forward + s * old_side, -450.0f, 450.0f);
        cmd.side_move = clampf(-s * old_forward + c * old_side, -450.0f, 450.0f);
    }

    void on_create_move(context& ctx, command& cmd)
    {
        if (!ctx.initialized)
            initialize(ctx);
        if (!ctx.game.get_local_player)
            return;

        player_state local{};
        if (!ctx.game.get_local_player(local) || !local.valid || !local.alive)
        {
            reset_runtime(ctx);
            return;
        }

        target_state targets[64]{};
        unsigned int target_count = 0u;
        if (ctx.game.get_targets)
        {
            target_count = ctx.game.get_targets(targets, 64u);
            if (target_count > 64u)
                target_count = 64u;
        }

        apply_bunnyhop(ctx.config.movement, local, cmd);
        apply_air_strafe(ctx.config.movement, local, cmd);
        apply_edge_features(ctx, local, cmd);
        apply_slowwalk(ctx.config.movement, local, cmd);
        apply_quickpeek(ctx, local, cmd);

        const bool rage_fired = apply_rage(ctx, local, cmd, targets, target_count);
        if (!rage_fired)
            apply_legit(ctx, local, cmd, targets, target_count);

        apply_autos(ctx, local, cmd);
        apply_antiaim(ctx, local, cmd, targets, target_count);

        ctx.runtime.previous_on_ground = local.on_ground;
        ctx.runtime.ticks_in_air = local.on_ground ? 0 : ctx.runtime.ticks_in_air + 1;
        ctx.runtime.last_command_number = cmd.command_number;
    }
}
