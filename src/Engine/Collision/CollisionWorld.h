#pragma once

#include <chlm/CarrotHLM.h>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace carrot::collision {
    using collision_layer_t = uint32_t;
    using collision_mask_t = uint32_t;

    inline constexpr collision_layer_t k_collision_layer_none{ 0u };
    inline constexpr collision_mask_t k_collision_mask_none{ 0u };
    inline constexpr collision_mask_t k_collision_mask_all{ 0xffffffffu };

    [[nodiscard]] constexpr collision_layer_t make_collision_layer(const uint32_t bit_index) noexcept
    {
        return bit_index < 32u ? (1u << bit_index) : 0u;
    }

    struct collision_query_filter_t
    {
        collision_layer_t layer{ k_collision_mask_all };
        collision_mask_t mask{ k_collision_mask_all };
    };

    struct collision_aabb_t
    {
        chlm::float2 min{ 0.f, 0.f };
        chlm::float2 max{ 0.f, 0.f };

        [[nodiscard]] chlm::float2 size() const noexcept { return max - min; }
        [[nodiscard]] chlm::float2 center() const noexcept { return (min + max) * 0.5f; }
        [[nodiscard]] chlm::float2 extents() const noexcept { return (max - min) * 0.5f; }
        [[nodiscard]] collision_aabb_t translated(const chlm::float2 offset) const noexcept
        {
            return collision_aabb_t{ .min = min + offset, .max = max + offset };
        }

        [[nodiscard]] static collision_aabb_t from_min_size(const chlm::float2 min,
                                                            const chlm::float2 size) noexcept
        {
            return collision_aabb_t{ .min = min, .max = min + size };
        }

        [[nodiscard]] static collision_aabb_t from_center_extents(const chlm::float2 center,
                                                                  const chlm::float2 extents) noexcept
        {
            return collision_aabb_t{ .min = center - extents, .max = center + extents };
        }
    };

    struct static_collider_t
    {
        enum class shape_t : uint8_t
        {
            aabb = 0,
            convex_polygon
        };

        uint64_t id{ 0 };
        shape_t shape{ shape_t::aabb };
        collision_aabb_t bounds{ };
        std::vector<chlm::float2> polygon_points;
        collision_layer_t layer{ make_collision_layer(0u) };
        collision_mask_t mask{ k_collision_mask_all };

        [[nodiscard]] bool is_aabb() const noexcept { return shape == shape_t::aabb; }
        [[nodiscard]] bool is_convex_polygon() const noexcept { return shape == shape_t::convex_polygon; }
    };

    struct tile_collision_cell_t
    {
        bool solid{ false };
        collision_layer_t layer{ make_collision_layer(0u) };
        collision_mask_t mask{ k_collision_mask_all };
    };

    class tile_collision_field_t
    {
    public:
        tile_collision_field_t() = default;
        tile_collision_field_t(uint32_t width,
                               uint32_t height,
                               chlm::float2 origin,
                               chlm::float2 cell_size);

        [[nodiscard]] uint64_t id() const noexcept { return _id; }
        [[nodiscard]] uint32_t width() const noexcept { return _width; }
        [[nodiscard]] uint32_t height() const noexcept { return _height; }
        [[nodiscard]] const chlm::float2& origin() const noexcept { return _origin; }
        [[nodiscard]] const chlm::float2& cell_size() const noexcept { return _cell_size; }

        void set_id(const uint64_t id) noexcept { _id = id; }
        void fill(tile_collision_cell_t cell);
        [[nodiscard]] bool contains_cell(uint32_t x, uint32_t y) const noexcept;
        [[nodiscard]] tile_collision_cell_t* cell_at(uint32_t x, uint32_t y) noexcept;
        [[nodiscard]] const tile_collision_cell_t* cell_at(uint32_t x, uint32_t y) const noexcept;
        [[nodiscard]] collision_aabb_t cell_bounds(uint32_t x, uint32_t y) const noexcept;

    private:
        [[nodiscard]] size_t index_of(uint32_t x, uint32_t y) const noexcept;

        uint64_t _id{ 0 };
        uint32_t _width{ 0 };
        uint32_t _height{ 0 };
        chlm::float2 _origin{ 0.f, 0.f };
        chlm::float2 _cell_size{ 1.f, 1.f };
        std::vector<tile_collision_cell_t> _cells;
    };

    struct collision_hit_ref_t
    {
        const static_collider_t* collider{ nullptr };
        const tile_collision_field_t* tile_field{ nullptr };
        uint32_t tile_x{ 0 };
        uint32_t tile_y{ 0 };
        collision_aabb_t bounds{ };
        collision_layer_t layer{ k_collision_layer_none };
        collision_mask_t mask{ k_collision_mask_none };

        [[nodiscard]] bool is_static_collider() const noexcept { return collider != nullptr; }
        [[nodiscard]] bool is_tile_cell() const noexcept { return tile_field != nullptr; }
    };

    struct raycast_hit_t
    {
        collision_hit_ref_t hit{ };
        chlm::float2 position{ 0.f, 0.f };
        chlm::float2 normal{ 0.f, 0.f };
        float fraction{ 0.f };
        float distance{ 0.f };
        bool started_overlapping{ false };
    };

    struct sweep_hit_t
    {
        collision_hit_ref_t hit{ };
        chlm::float2 position{ 0.f, 0.f };
        chlm::float2 normal{ 0.f, 0.f };
        float fraction{ 0.f };
        float distance{ 0.f };
        bool started_overlapping{ false };
    };

    enum class collision_query_kind_t : uint8_t
    {
        none = 0,
        point,
        overlap,
        raycast,
        sweep_aabb
    };

    struct collision_query_debug_stats_t
    {
        collision_query_kind_t kind{ collision_query_kind_t::none };
        uint32_t static_candidate_count{ 0u };
        uint32_t static_tested_count{ 0u };
        uint32_t tile_candidate_count{ 0u };
        uint32_t tile_tested_count{ 0u };
        uint32_t hit_count{ 0u };
        bool found_blocking_hit{ false };
    };

    struct collision_broadphase_debug_summary_t
    {
        float static_cell_size_world{ 1.f };
        uint32_t static_bucket_count{ 0u };
        uint32_t static_indexed_entry_count{ 0u };
        collision_query_debug_stats_t last_query;
    };

    class collision_world_t
    {
    public:
        void clear() noexcept;

        [[nodiscard]] const static_collider_t& add_static_collider(static_collider_t collider);
        [[nodiscard]] tile_collision_field_t& create_tile_collision_field(uint32_t width,
                                                                          uint32_t height,
                                                                          chlm::float2 origin,
                                                                          chlm::float2 cell_size);

        [[nodiscard]] const std::vector<static_collider_t>& static_colliders() const noexcept { return _static_colliders; }
        [[nodiscard]] std::vector<static_collider_t>& static_colliders() noexcept { return _static_colliders; }
        [[nodiscard]] const std::vector<tile_collision_field_t>& tile_fields() const noexcept { return _tile_fields; }
        [[nodiscard]] std::vector<tile_collision_field_t>& tile_fields() noexcept { return _tile_fields; }
        [[nodiscard]] const collision_broadphase_debug_summary_t& broadphase_debug_summary() const noexcept
        {
            return _broadphase_debug_summary;
        }

        [[nodiscard]] std::vector<collision_hit_ref_t> point_query(chlm::float2 point,
                                                                   collision_query_filter_t filter = {}) const;
        [[nodiscard]] std::vector<collision_hit_ref_t> overlap_query(
            const collision_aabb_t& bounds,
            collision_query_filter_t filter = {}) const;
        [[nodiscard]] std::optional<raycast_hit_t> raycast(chlm::float2 origin,
                                                           chlm::float2 delta,
                                                           collision_query_filter_t filter = {}) const;
        [[nodiscard]] std::optional<sweep_hit_t> sweep_aabb(const collision_aabb_t& moving_bounds,
                                                            chlm::float2 delta,
                                                            collision_query_filter_t filter = {}) const;

    private:
        struct static_broadphase_cell_key_t
        {
            int32_t x{ 0 };
            int32_t y{ 0 };

            [[nodiscard]] bool operator==(const static_broadphase_cell_key_t& other) const noexcept
            {
                return x == other.x && y == other.y;
            }
        };

        struct static_broadphase_cell_key_hasher_t
        {
            [[nodiscard]] size_t operator()(const static_broadphase_cell_key_t& key) const noexcept;
        };

        void index_static_collider(size_t collider_index);
        void record_query_debug_stats(collision_query_debug_stats_t stats) const noexcept;

        uint64_t _next_static_collider_id{ 1 };
        uint64_t _next_tile_field_id{ 1 };
        std::vector<static_collider_t> _static_colliders;
        std::vector<tile_collision_field_t> _tile_fields;
        std::unordered_map<static_broadphase_cell_key_t,
                           std::vector<size_t>,
                           static_broadphase_cell_key_hasher_t> _static_broadphase;
        uint32_t _static_broadphase_entry_count{ 0u };
        mutable collision_broadphase_debug_summary_t _broadphase_debug_summary;
    };

    [[nodiscard]] bool collision_layers_match(collision_layer_t candidate_layer,
                                              collision_mask_t candidate_mask,
                                              collision_query_filter_t filter) noexcept;
    [[nodiscard]] bool collision_aabb_contains_point(const collision_aabb_t& bounds, chlm::float2 point) noexcept;
    [[nodiscard]] bool collision_aabb_overlaps(const collision_aabb_t& a, const collision_aabb_t& b) noexcept;
} // namespace carrot::collision
