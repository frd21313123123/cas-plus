#pragma once

// Clean-room compatibility layer inspired by the public Velocity feature surface.
// This file intentionally contains no copied Velocity implementation code.
// It keeps the core independent from CS2 offsets/hooks so the adapter can fail closed.

namespace cas_velocity
{
    struct vec3
    {
        float x{};
        float y{};
        float z{};
    };

    enum class pitch_mode : unsigned char { none = 0, down, up };
    enum class autoyaw_mode : unsigned char { none = 0, crosshair, distance, health };
    enum class manual_direction : signed char { none = 0, left = -1, right = 1 };
    enum class weapon_group : unsigned char { pistol = 0, smg, rifle, shotgun, sniper, lmg, count };

    struct rage_weapon_settings
    {
        bool silent{ true };
        bool no_spread{};
        bool doubletap{};
        bool body_aim{};
        bool force_shot_air{};
        bool force_shot_ground{};
        bool auto_stop{ true };
        float max_fov{ 180.0f };
        int hitchance{ 80 };
        int min_damage{ 101 };
        bool min_damage_override{};
        int min_damage_override_value{ 11 };
        bool hitchance_override{};
        int hitchance_override_value{ 75 };
        float point_scale{ 85.0f };
        bool dynamic_point_scale{ true };
        bool hitboxes[6]{ true, true, true, true, true, true };
    };

    struct rage_settings
    {
        bool enabled{ true };
        rage_weapon_settings groups[static_cast<unsigned int>(weapon_group::count)]{};
        int max_backtrack_ticks{ 12 };
        bool extrapolation{ true };
        int max_extrapolate_ticks{ 8 };
    };

    struct legit_weapon_settings
    {
        bool aimbot{};
        float fov{ 5.0f };
        int smooth{ 5 };
        bool hitboxes[5]{ true, false, false, false, false };
        bool recoil_control{ true };
        int recoil_min{ 95 };
        int recoil_max{ 105 };
        bool standalone_rcs{};
        int standalone_rcs_strength{ 100 };
        bool triggerbot{};
        int trigger_delay_ms{ 5 };
        int trigger_hitchance{ 80 };
        bool trigger_head_only{};
        bool trigger_seed_mode{};
        bool autowall{ true };
        int min_damage{ 101 };
        bool visualize_fov{ true };
    };

    struct legit_settings
    {
        bool enabled{};
        legit_weapon_settings groups[static_cast<unsigned int>(weapon_group::count)]{};
    };

    struct antiaim_settings
    {
        bool enabled{ true };
        pitch_mode pitch{ pitch_mode::down };
        autoyaw_mode autoyaw{ autoyaw_mode::none };
        bool at_targets{};
        bool auto_yaw_adjust{ true };
        manual_direction manual{ manual_direction::none };
        bool hide_shots{ true };
        bool avoid_backstab{ true };
        bool direction_indicator{ true };
        bool direction_indicator_glow{ true };
        float direction_indicator_glow_strength{ 0.55f };
    };

    struct movement_settings
    {
        bool bunnyhop{};
        bool air_strafe{};
        bool edge_bug{};
        bool edge_jump{};
        bool edge_stop{};
        bool jump_bug{};
        bool fast_ladder{};
        bool slow_walk{};
        float slow_walk_speed{ 80.0f };
    };

    struct assistance_settings
    {
        bool quick_peek{};
        bool duck_peek{};
        bool auto_revolver{ true };
        bool auto_scope{ true };
        bool zeusbot{ true };
        bool zeus_drop_after{ true };
        bool knifebot{ true };
        bool penetration_crosshair{};
    };

    struct world_settings
    {
        bool smoke_removal{};
        bool weather{};
        bool scene_modulation{};
        bool projectile_trajectory{};
        bool dynamic_lights{};
        bool bullet_impacts{};
        bool scoreboard_weapons{};
        bool preserve_killfeed{};
    };

    struct settings
    {
        rage_settings rage{};
        legit_settings legit{};
        antiaim_settings antiaim{};
        movement_settings movement{};
        assistance_settings assistance{};
        world_settings world{};
    };

    enum command_buttons : unsigned int
    {
        button_attack = 1u << 0,
        button_jump = 1u << 1,
        button_duck = 1u << 2,
        button_forward = 1u << 3,
        button_back = 1u << 4,
        button_use = 1u << 5,
        button_left = 1u << 7,
        button_right = 1u << 8,
        button_moveleft = 1u << 9,
        button_moveright = 1u << 10,
        button_attack2 = 1u << 11
    };

    struct command
    {
        unsigned int buttons{};
        float pitch{};
        float yaw{};
        float forward_move{};
        float side_move{};
        float up_move{};
        int command_number{};
        int tick_count{};
    };

    struct player_state
    {
        bool valid{};
        bool alive{};
        bool on_ground{};
        bool on_ladder{};
        bool scoped{};
        bool can_fire{};
        bool weapon_is_revolver{};
        bool weapon_is_sniper{};
        bool weapon_is_knife{};
        bool weapon_is_zeus{};
        int health{};
        weapon_group weapon{ weapon_group::rifle };
        vec3 origin{};
        vec3 velocity{};
        float view_pitch{};
        float view_yaw{};
        float recoil_pitch{};
        float recoil_yaw{};
        float weapon_max_speed{ 250.0f };
        float tick_interval{ 0.015625f };
    };

    struct target_state
    {
        bool valid{};
        bool alive{};
        bool visible{};
        bool knife_threat{};
        bool melee_reachable{};
        bool zeus_reachable{};
        int health{};
        int estimated_damage{};
        int estimated_hitchance{};
        int backtrack_ticks{};
        float distance_sqr{};
        float crosshair_delta_sqr{};
        float aim_pitch{};
        float aim_yaw{};
        vec3 origin{};
        vec3 velocity{};
    };

    struct runtime_state
    {
        bool quick_peek_has_origin{};
        bool quick_peek_retracking{};
        vec3 quick_peek_origin{};
        bool previous_on_ground{};
        bool rage_fired_this_tick{};
        bool duckpeek_fake_stand{};
        int ticks_in_air{};
        int doubletap_charge{};
        int last_command_number{};
        int trigger_hold_ticks{};
        float last_real_yaw{};
        float last_fake_yaw{};
    };

    struct adapter
    {
        bool (*get_local_player)(player_state& out){};
        unsigned int (*get_targets)(target_state* out, unsigned int capacity){};
        bool (*trace_world)(const vec3& from, const vec3& to, float& fraction){};
        bool (*can_penetrate)(const vec3& from, const vec3& to, int& damage){};
        void (*request_drop_weapon)(){};
        void (*set_scope)(bool scoped){};

        void (*on_lag_records)(){};
        void (*on_chams_history)(){};
        void (*on_scoreboard_weapons)(){};
        void (*on_changer_guns)(){};
        void (*on_changer_cosmetics)(){};
        void (*on_world_scene)(){};
        void (*on_weather)(){};
        void (*on_impacts)(){};
        void (*on_misc_stage)(){};
        void (*on_killfeed_preserve)(){};
    };

    struct context
    {
        settings config{};
        runtime_state runtime{};
        adapter game{};
        bool initialized{};
    };

    void initialize(context& ctx);
    void reset_runtime(context& ctx);
    void on_create_move(context& ctx, command& cmd);
    void on_frame_stage(context& ctx, int stage, bool after_original);

    float normalize_yaw(float yaw);
    float clamp_pitch(float pitch);
    vec3 extrapolate(const vec3& origin, const vec3& velocity, float interval, int ticks);
    void correct_movement(command& cmd, float old_yaw, float new_yaw);
}
