#include "velocity_port.hpp"

namespace cas_velocity
{
    namespace
    {
        constexpr float k_pi = 3.14159265358979323846f;
        constexpr float k_deg_to_rad = k_pi / 180.0f;

        float absf(float v) { return v < 0.0f ? -v : v; }
        float maxf(float a, float b) { return a > b ? a : b; }
        float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

        float wrap_radians(float v)
        {
            while (v > k_pi) v -= 2.0f * k_pi;
            while (v < -k_pi) v += 2.0f * k_pi;
            return v;
        }

        float fast_sin(float x)
        {
            x = wrap_radians(x);
            const float x2 = x * x;
            return x * (1.0f - x2 / 6.0f + (x2 * x2) / 120.0f - (x2 * x2 * x2) / 5040.0f);
        }

        float fast_cos(float x)
        {
            x = wrap_radians(x);
            const float x2 = x * x;
            return 1.0f - x2 / 2.0f + (x2 * x2) / 24.0f - (x2 * x2 * x2) / 720.0f;
        }

        float sqrt_approx(float value)
        {
            if (value <= 0.0f) return 0.0f;
            float x = value > 1.0f ? value : 1.0f;
            for (int i = 0; i < 7; ++i) x = 0.5f * (x + value / x);
            return x;
        }

        float length2d_sqr(const vec3& v) { return v.x * v.x + v.y * v.y; }

        rage_weapon_settings& rage_group(settings& cfg, weapon_group group)
        {
            unsigned int i = static_cast<unsigned int>(group);
            const unsigned int count = static_cast<unsigned int>(weapon_group::count);
            if (i >= count) i = static_cast<unsigned int>(weapon_group::rifle);
            return cfg.rage.groups[i];
        }

        legit_weapon_settings& legit_group(settings& cfg, weapon_group group)
        {
            unsigned int i = static_cast<unsigned int>(group);
            const unsigned int count = static_cast<unsigned int>(weapon_group::count);
            if (i >= count) i = static_cast<unsigned int>(weapon_group::rifle);
            return cfg.legit.groups[i];
        }

        bool should_skip_antiaim(const player_state& local, const command& cmd)
        {
            if (!local.valid || !local.alive || local.on_ladder) return true;
            if ((cmd.buttons & button_use) != 0u) return true;
            if ((cmd.buttons & button_attack) != 0u && local.can_fire) return true;
            return false;
        }

        int select_target(const target_state* targets, unsigned int count, autoyaw_mode mode)
        {
            int selected = -1;
            float best_float = 0.0f;
            int best_health = 0;
            for (unsigned int i = 0; i < count; ++i)
            {
                const target_state& t = targets[i];
                if (!t.valid || !t.alive) continue;
                if (selected < 0)
                {
                    selected = static_cast<int>(i);
                    best_float = mode == autoyaw_mode::distance ? t.distance_sqr : t.crosshair_delta_sqr;
                    best_health = t.health;
                    continue;
                }
                if (mode == autoyaw_mode::health)
                {
                    if (t.health < best_health) { selected = static_cast<int>(i); best_health = t.health; }
                }
                else
                {
                    const float metric = mode == autoyaw_mode::distance ? t.distance_sqr : t.crosshair_delta_sqr;
                    if (metric < best_float) { selected = static_cast<int>(i); best_float = metric; }
                }
            }
            return selected;
        }

        int select_knife_threat(const target_state* targets, unsigned int count)
        {
            int selected = -1;
            float best = 0.0f;
            for (unsigned int i = 0; i < count; ++i)
            {
                const target_state& t = targets[i];
                if (!t.valid || !t.alive || !t.knife_threat) continue;
                if (selected < 0 || t.distance_sqr < best)
                {
                    selected = static_cast<int>(i);
                    best = t.distance_sqr;
                }
            }
            return selected;
        }

        const rage_weapon_settings& rage_group_const(const context& ctx, weapon_group group)
        {
            unsigned int i = static_cast<unsigned int>(group);
            const unsigned int count = static_cast<unsigned int>(weapon_group::count);
            if (i >= count) i = static_cast<unsigned int>(weapon_group::rifle);
            return ctx.config.rage.groups[i];
        }

        bool target_passes_rage(const context& ctx, const player_state& local, const target_state& t)
        {
            if (!t.valid || !t.alive) return false;
            const rage_weapon_settings& group = rage_group_const(ctx, local.weapon);
            const int min_damage = group.min_damage_override ? group.min_damage_override_value : group.min_damage;
            const int min_hc = group.hitchance_override ? group.hitchance_override_value : group.hitchance;
            if (t.crosshair_delta_sqr > group.max_fov * group.max_fov) return false;
            if (t.estimated_hitchance < min_hc) return false;
            if (t.estimated_damage < min_damage && t.estimated_damage < t.health) return false;
            if (t.backtrack_ticks > ctx.config.rage.max_backtrack_ticks) return false;
            return true;
        }

        bool has_viable_rage_target(const context& ctx, const player_state& local, const target_state* targets, unsigned int count)
        {
            if (!ctx.config.rage.enabled) return false;
            for (unsigned int i = 0; i < count; ++i)
                if (target_passes_rage(ctx, local, targets[i])) return true;
            return false;
        }

        void apply_antiaim(context& ctx, const player_state& local, command& cmd, target_state* targets, unsigned int count)
        {
            antiaim_settings& cfg = ctx.config.antiaim;
            if (!cfg.enabled || should_skip_antiaim(local, cmd)) return;

            if (cfg.pitch == pitch_mode::down) cmd.pitch = 89.0f;
            else if (cfg.pitch == pitch_mode::up) cmd.pitch = -89.0f;

            float yaw = local.view_yaw + 180.0f;
            const int knife = cfg.avoid_backstab ? select_knife_threat(targets, count) : -1;
            if (knife >= 0)
            {
                yaw = targets[knife].aim_yaw;
            }
            else if (cfg.manual == manual_direction::left)
            {
                yaw = local.view_yaw - 90.0f;
            }
            else if (cfg.manual == manual_direction::right)
            {
                yaw = local.view_yaw + 90.0f;
            }
            else if ((cfg.at_targets || cfg.autoyaw != autoyaw_mode::none) && count > 0u)
            {
                autoyaw_mode mode = cfg.autoyaw == autoyaw_mode::none ? autoyaw_mode::crosshair : cfg.autoyaw;
                const int selected = select_target(targets, count, mode);
                if (selected >= 0) yaw = targets[selected].aim_yaw + 180.0f;
            }

            if (cfg.auto_yaw_adjust) yaw += 33.0f;
            const float old_yaw = cmd.yaw;
            cmd.yaw = normalize_yaw(yaw);
            correct_movement(cmd, old_yaw, cmd.yaw);
            ctx.runtime.last_real_yaw = cmd.yaw;
            ctx.runtime.last_fake_yaw = normalize_yaw(cmd.yaw + 180.0f);
        }

        void apply_autostop(context& ctx, const player_state& local, command& cmd, target_state* targets, unsigned int count)
        {
            rage_weapon_settings& group = rage_group(ctx.config, local.weapon);
            if (!group.auto_stop || !local.on_ground || !has_viable_rage_target(ctx, local, targets, count)) return;
            if (length2d_sqr(local.velocity) < 16.0f)
            {
                cmd.forward_move = 0.0f;
                cmd.side_move = 0.0f;
                return;
            }

            const float yaw = cmd.yaw * k_deg_to_rad;
            const float fx = fast_cos(yaw), fy = fast_sin(yaw);
            const float rx = -fy, ry = fx;
            cmd.forward_move = clampf(-(local.velocity.x * fx + local.velocity.y * fy), -450.0f, 450.0f);
            cmd.side_move = clampf(-(local.velocity.x * rx + local.velocity.y * ry), -450.0f, 450.0f);
        }

        void apply_slowwalk(const movement_settings& cfg, const player_state& local, command& cmd)
        {
            if (!cfg.slow_walk || !local.on_ground) return;
            const float move_sqr = cmd.forward_move * cmd.forward_move + cmd.side_move * cmd.side_move;
            const float limit = maxf(cfg.slow_walk_speed, 1.0f);
            if (move_sqr <= limit * limit || move_sqr <= 0.001f) return;
            const float length = sqrt_approx(move_sqr);
            if (length <= 0.001f) return;
            const float scale = limit / length;
            cmd.forward_move *= scale;
            cmd.side_move *= scale;
        }

        void apply_bunnyhop(const movement_settings& cfg, const player_state& local, command& cmd)
        {
            if (!cfg.bunnyhop || local.on_ladder) return;
            if ((cmd.buttons & button_jump) != 0u && !local.on_ground) cmd.buttons &= ~button_jump;
        }

        void apply_edge_features(context& ctx, const player_state& local, command& cmd)
        {
            const movement_settings& cfg = ctx.config.movement;
            if (cfg.edge_jump && ctx.runtime.previous_on_ground && !local.on_ground) cmd.buttons |= button_jump;
            if (cfg.jump_bug && ctx.runtime.previous_on_ground && !local.on_ground)
            {
                cmd.buttons &= ~button_jump;
                cmd.buttons |= button_duck;
            }
            if (cfg.edgestop && local.on_ground && length2d_sqr(local.velocity) > 2500.0f)
            {
                cmd.forward_move = -local.velocity.x;
                cmd.side_move = -local.velocity.y;
            }
            if (cfg.edge_bug && !local.on_ground && local.velocity.z < -80.0f)
            {
                cmd.buttons |= button_duck;
                cmd.buttons &= ~button_jump;
            }
        }

        void apply_fast_ladder(const movement_settings& cfg, const player_state& local, command& cmd)
        {
            if (!cfg.fast_ladder || !local.on_ladder) return;
            if (absf(cmd.forward_move) > 1.0f) cmd.forward_move = cmd.forward_move > 0.0f ? 450.0f : -450.0f;
            if (absf(cmd.side_move) < 1.0f) cmd.side_move = (cmd.command_number & 1) ? 450.0f : -450.0f;
        }

        void apply_duckpeek(context& ctx, command& cmd)
        {
            if (!ctx.config.assistance.duck_peek)
            {
                ctx.runtime.duckpeek_fake_stand = false;
                return;
            }
            cmd.buttons |= button_duck;
            ctx.runtime.duckpeek_fake_stand = false;
        }

        bool apply_rage(context& ctx, const player_state& local, command& cmd, target_state* targets, unsigned int count)
        {
            if (!ctx.config.rage.enabled || !local.can_fire || count == 0u) return false;
            rage_weapon_settings& group = rage_group(ctx.config, local.weapon);
            int selected = -1;
            float best = group.max_fov * group.max_fov;
            for (unsigned int i = 0; i < count; ++i)
            {
                if (!target_passes_rage(ctx, local, targets[i])) continue;
                if (targets[i].crosshair_delta_sqr <= best)
                {
                    selected = static_cast<int>(i);
                    best = targets[i].crosshair_delta_sqr;
                }
            }
            if (selected < 0) return false;

            const float old_yaw = cmd.yaw;
            cmd.pitch = clamp_pitch(targets[selected].aim_pitch);
            cmd.yaw = normalize_yaw(targets[selected].aim_yaw);
            correct_movement(cmd, old_yaw, cmd.yaw);
            cmd.buttons |= button_attack;
            if (ctx.config.assistance.duck_peek)
            {
                cmd.buttons &= ~button_duck;
                ctx.runtime.duckpeek_fake_stand = true;
            }
            if (group.doubletap)
            {
                if (ctx.runtime.doubletap_charge < 14) ++ctx.runtime.doubletap_charge;
                else ctx.runtime.doubletap_charge = 0;
            }
            return true;
        }

        void apply_legit(context& ctx, const player_state& local, command& cmd, target_state* targets, unsigned int count)
        {
            if (!ctx.config.legit.enabled) return;
            legit_weapon_settings& group = legit_group(ctx.config, local.weapon);
            int selected = -1;
            float best = group.fov * group.fov;
            for (unsigned int i = 0; i < count; ++i)
            {
                const target_state& t = targets[i];
                if (!t.valid || !t.alive) continue;
                if (!group.autowall && !t.visible) continue;
                if (t.estimated_damage < group.min_damage && t.estimated_damage < t.health) continue;
                if (t.crosshair_delta_sqr < best) { selected = static_cast<int>(i); best = t.crosshair_delta_sqr; }
            }

            const float rcs_average = 0.005f * static_cast<float>(group.recoil_min + group.recoil_max);
            if (selected >= 0 && group.aimbot)
            {
                const int smooth = group.smooth < 1 ? 1 : group.smooth;
                const target_state& t = targets[selected];
                float desired_pitch = t.aim_pitch;
                float desired_yaw = t.aim_yaw;
                if (group.recoil_control)
                {
                    desired_pitch -= local.recoil_pitch * rcs_average;
                    desired_yaw -= local.recoil_yaw * rcs_average;
                }
                const float old_yaw = cmd.yaw;
                cmd.yaw = normalize_yaw(cmd.yaw + normalize_yaw(desired_yaw - cmd.yaw) / static_cast<float>(smooth));
                cmd.pitch = clamp_pitch(cmd.pitch + clamp_pitch(desired_pitch - cmd.pitch) / static_cast<float>(smooth));
                correct_movement(cmd, old_yaw, cmd.yaw);
            }
            else if (group.standalone_rcs)
            {
                const float strength = clampf(static_cast<float>(group.standalone_rcs_strength) / 100.0f, 0.0f, 2.0f);
                cmd.pitch = clamp_pitch(cmd.pitch - local.recoil_pitch * strength);
                cmd.yaw = normalize_yaw(cmd.yaw - local.recoil_yaw * strength);
            }

            if (selected >= 0 && group.triggerbot && targets[selected].estimated_hitchance >= group.trigger_hitchance && local.can_fire)
            {
                ++ctx.runtime.trigger_hold_ticks;
                const float interval_ms = maxf(local.tick_interval * 1000.0f, 1.0f);
                const int required = group.trigger_delay_ms <= 0 ? 0 : static_cast<int>(static_cast<float>(group.trigger_delay_ms) / interval_ms + 0.999f);
                if (ctx.runtime.trigger_hold_ticks >= required) cmd.buttons |= button_attack;
            }
            else ctx.runtime.trigger_hold_ticks = 0;
        }

        bool apply_special_bots(context& ctx, const player_state& local, command& cmd, target_state* targets, unsigned int count)
        {
            if (!local.can_fire) return false;
            if (local.weapon_is_zeus && ctx.config.assistance.zeusbot)
            {
                for (unsigned int i = 0; i < count; ++i)
                {
                    if (!targets[i].valid || !targets[i].alive || !targets[i].zeus_reachable) continue;
                    cmd.pitch = clamp_pitch(targets[i].aim_pitch);
                    cmd.yaw = normalize_yaw(targets[i].aim_yaw);
                    cmd.buttons |= button_attack;
                    if (ctx.config.assistance.zeus_drop_after && ctx.game.request_drop_weapon) ctx.game.request_drop_weapon();
                    return true;
                }
            }
            if (local.weapon_is_knife && ctx.config.assistance.knifebot)
            {
                for (unsigned int i = 0; i < count; ++i)
                {
                    if (!targets[i].valid || !targets[i].alive || !targets[i].melee_reachable) continue;
                    cmd.pitch = clamp_pitch(targets[i].aim_pitch);
                    cmd.yaw = normalize_yaw(targets[i].aim_yaw);
                    cmd.buttons |= button_attack2;
                    return true;
                }
            }
            return false;
        }

        void apply_autos(context& ctx, const player_state& local, command& cmd)
        {
            if (ctx.config.assistance.auto_scope && local.weapon_is_sniper && !local.scoped && (cmd.buttons & button_attack) != 0u)
            {
                cmd.buttons &= ~button_attack;
                cmd.buttons |= button_attack2;
                if (ctx.game.set_scope) ctx.game.set_scope(true);
            }
            if (ctx.config.assistance.auto_revolver && local.weapon_is_revolver && local.can_fire) cmd.buttons |= button_attack;
        }

        void apply_air_strafe(const movement_settings& cfg, const player_state& local, command& cmd, bool firing)
        {
            if (!cfg.air_strafe || local.on_ground || local.on_ladder || firing) return;
            if (length2d_sqr(local.velocity) < 4.0f) return;
            if (absf(cmd.side_move) < 1.0f) cmd.side_move = (cmd.command_number & 1) ? 450.0f : -450.0f;
            cmd.forward_move = 0.0f;
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
            if ((cmd.buttons & button_attack) != 0u && local.can_fire) ctx.runtime.quick_peek_retracking = true;
            if (!ctx.runtime.quick_peek_retracking || !ctx.runtime.quick_peek_has_origin) return;

            const float dx = ctx.runtime.quick_peek_origin.x - local.origin.x;
            const float dy = ctx.runtime.quick_peek_origin.y - local.origin.y;
            if (dx * dx + dy * dy < 25.0f)
            {
                ctx.runtime.quick_peek_retracking = false;
                cmd.forward_move = 0.0f;
                cmd.side_move = 0.0f;
                return;
            }
            const float yaw = cmd.yaw * k_deg_to_rad;
            const float fx = fast_cos(yaw), fy = fast_sin(yaw);
            const float rx = -fy, ry = fx;
            cmd.forward_move = clampf(dx * fx + dy * fy, -450.0f, 450.0f);
            cmd.side_move = clampf(dx * rx + dy * ry, -450.0f, 450.0f);
        }
    }

    void initialize(context& ctx)
    {
        reset_runtime(ctx);
        ctx.initialized = true;
    }

    void reset_runtime(context& ctx) { ctx.runtime = runtime_state{}; }

    float normalize_yaw(float yaw)
    {
        while (yaw > 180.0f) yaw -= 360.0f;
        while (yaw < -180.0f) yaw += 360.0f;
        return yaw;
    }

    float clamp_pitch(float pitch) { return clampf(pitch, -89.0f, 89.0f); }

    vec3 extrapolate(const vec3& origin, const vec3& velocity, float interval, int ticks)
    {
        if (ticks < 0) ticks = 0;
        const float time = interval * static_cast<float>(ticks);
        return { origin.x + velocity.x * time, origin.y + velocity.y * time, origin.z + velocity.z * time };
    }

    void correct_movement(command& cmd, float old_yaw, float new_yaw)
    {
        const float delta = normalize_yaw(new_yaw - old_yaw) * k_deg_to_rad;
        const float c = fast_cos(delta), s = fast_sin(delta);
        const float f = cmd.forward_move, side = cmd.side_move;
        cmd.forward_move = clampf(c * f + s * side, -450.0f, 450.0f);
        cmd.side_move = clampf(-s * f + c * side, -450.0f, 450.0f);
    }

    void on_create_move(context& ctx, command& cmd)
    {
        if (!ctx.initialized) initialize(ctx);
        if (!ctx.game.get_local_player) return;

        player_state local{};
        if (!ctx.game.get_local_player(local) || !local.valid || !local.alive)
        {
            reset_runtime(ctx);
            return;
        }

        target_state targets[64]{};
        unsigned int count = 0u;
        if (ctx.game.get_targets)
        {
            count = ctx.game.get_targets(targets, 64u);
            if (count > 64u) count = 64u;
        }

        ctx.runtime.rage_fired_this_tick = false;

        // Velocity-compatible order: shared/AA/autostop, movement, combat, duck/airstrafe, quickpeek.
        apply_antiaim(ctx, local, cmd, targets, count);
        apply_autostop(ctx, local, cmd, targets, count);

        apply_slowwalk(ctx.config.movement, local, cmd);
        apply_edge_features(ctx, local, cmd);
        apply_bunnyhop(ctx.config.movement, local, cmd);
        apply_fast_ladder(ctx.config.movement, local, cmd);

        const bool special_fired = apply_special_bots(ctx, local, cmd, targets, count);
        const bool rage_fired = special_fired ? true : apply_rage(ctx, local, cmd, targets, count);
        ctx.runtime.rage_fired_this_tick = rage_fired;
        if (!rage_fired) apply_legit(ctx, local, cmd, targets, count);

        apply_duckpeek(ctx, cmd);
        if (rage_fired && ctx.config.assistance.duck_peek)
        {
            cmd.buttons &= ~button_duck;
            ctx.runtime.duckpeek_fake_stand = true;
        }
        apply_air_strafe(ctx.config.movement, local, cmd, rage_fired);
        apply_autos(ctx, local, cmd);
        apply_quickpeek(ctx, local, cmd);

        ctx.runtime.previous_on_ground = local.on_ground;
        ctx.runtime.ticks_in_air = local.on_ground ? 0 : ctx.runtime.ticks_in_air + 1;
        ctx.runtime.last_command_number = cmd.command_number;
    }

    void on_frame_stage(context& ctx, int stage, bool after_original)
    {
        if (!ctx.initialized) initialize(ctx);
        if (!after_original)
        {
            if (stage == 6)
            {
                if (ctx.game.on_changer_guns) ctx.game.on_changer_guns();
                if (ctx.game.on_lag_records) ctx.game.on_lag_records();
                if (ctx.game.on_chams_history) ctx.game.on_chams_history();
                if (ctx.config.world.scoreboard_weapons && ctx.game.on_scoreboard_weapons) ctx.game.on_scoreboard_weapons();
            }
            else if (stage == 7)
            {
                if (ctx.game.on_changer_cosmetics) ctx.game.on_changer_cosmetics();
                if (ctx.config.world.scene_modulation && ctx.game.on_world_scene) ctx.game.on_world_scene();
                if (ctx.config.world.weather && ctx.game.on_weather) ctx.game.on_weather();
                if (ctx.game.on_misc_stage) ctx.game.on_misc_stage();
                if (ctx.config.world.bullet_impacts && ctx.game.on_impacts) ctx.game.on_impacts();
            }
        }
        else if (stage == 6 && ctx.config.world.preserve_killfeed && ctx.game.on_killfeed_preserve)
        {
            ctx.game.on_killfeed_preserve();
        }
    }
}
