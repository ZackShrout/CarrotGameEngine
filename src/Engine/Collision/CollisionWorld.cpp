//
// Created by Zack Shrout on 4/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "CollisionWorld.h"

namespace carrot::collision {
    namespace {
        constexpr float k_fraction_epsilon{ 1.0e-6f };

        struct ray_vs_aabb_result_t
        {
            bool hit{ false };
            bool started_overlapping{ false };
            float fraction{ 0.f };
            chlm::float2 normal{ 0.f, 0.f };
        };

        [[nodiscard]] float vector_length(const chlm::float2 value) noexcept
        {
            return std::sqrt((value.x * value.x) + (value.y * value.y));
        }

        [[nodiscard]] bool nearly_equal_fraction(const float lhs, const float rhs) noexcept
        {
            return std::fabs(lhs - rhs) <= k_fraction_epsilon;
        }

        [[nodiscard]] chlm::float2 normalize_or_zero(const chlm::float2 value) noexcept
        {
            const float length_sq{ (value.x * value.x) + (value.y * value.y) };
            if (length_sq <= 1.0e-12f)
                return { 0.f, 0.f };

            const float length{ std::sqrt(length_sq) };
            return value / length;
        }

        [[nodiscard]] collision_aabb_t bounds_for_points(const std::vector<chlm::float2>& points) noexcept
        {
            if (points.empty())
                return {};

            chlm::float2 min{ points.front() };
            chlm::float2 max{ points.front() };
            for (const chlm::float2 point : points)
            {
                min.x = std::min(min.x, point.x);
                min.y = std::min(min.y, point.y);
                max.x = std::max(max.x, point.x);
                max.y = std::max(max.y, point.y);
            }

            return collision_aabb_t{ .min = min, .max = max };
        }

        [[nodiscard]] bool interval_overlaps_strictly(const float min_a,
                                                      const float max_a,
                                                      const float min_b,
                                                      const float max_b) noexcept
        {
            return min_a < max_b && max_a > min_b;
        }

        [[nodiscard]] bool point_in_polygon(const chlm::float2 point,
                                            const std::vector<chlm::float2>& polygon_points) noexcept
        {
            if (polygon_points.size() < 3u)
                return false;

            bool inside{ false };
            for (size_t i{ 0u }, j{ polygon_points.size() - 1u }; i < polygon_points.size(); j = i++)
            {
                const chlm::float2 a{ polygon_points[i] };
                const chlm::float2 b{ polygon_points[j] };
                const bool intersects{
                    ((a.y > point.y) != (b.y > point.y)) &&
                    (point.x < ((b.x - a.x) * (point.y - a.y) / ((b.y - a.y) + 1.0e-12f)) + a.x)
                };
                if (intersects)
                    inside = !inside;
            }

            return inside;
        }

        struct projection_interval_t
        {
            float min{ 0.f };
            float max{ 0.f };
        };

        [[nodiscard]] projection_interval_t project_polygon(const std::vector<chlm::float2>& points,
                                                            const chlm::float2 axis) noexcept
        {
            const float first{ (points.front().x * axis.x) + (points.front().y * axis.y) };
            projection_interval_t projection{ .min = first, .max = first };
            for (size_t i{ 1u }; i < points.size(); ++i)
            {
                const float projected{ (points[i].x * axis.x) + (points[i].y * axis.y) };
                projection.min = std::min(projection.min, projected);
                projection.max = std::max(projection.max, projected);
            }
            return projection;
        }

        [[nodiscard]] projection_interval_t project_aabb(const chlm::float2 center,
                                                         const chlm::float2 extents,
                                                         const chlm::float2 axis) noexcept
        {
            const float projected_center{ (center.x * axis.x) + (center.y * axis.y) };
            const float projected_radius{ (std::fabs(axis.x) * extents.x) + (std::fabs(axis.y) * extents.y) };
            return projection_interval_t{
                .min = projected_center - projected_radius,
                .max = projected_center + projected_radius
            };
        }

        [[nodiscard]] std::vector<chlm::float2> polygon_axes(const std::vector<chlm::float2>& points)
        {
            std::vector<chlm::float2> axes;
            axes.reserve(points.size() + 2u);
            axes.push_back(chlm::float2{ 1.f, 0.f });
            axes.push_back(chlm::float2{ 0.f, 1.f });
            for (size_t i{ 0u }; i < points.size(); ++i)
            {
                const chlm::float2 edge{ points[(i + 1u) % points.size()] - points[i] };
                const chlm::float2 axis{ normalize_or_zero(chlm::float2{ -edge.y, edge.x }) };
                if (std::fabs(axis.x) <= 1.0e-6f && std::fabs(axis.y) <= 1.0e-6f)
                    continue;
                axes.push_back(axis);
            }
            return axes;
        }

        [[nodiscard]] bool aabb_overlaps_polygon(const collision_aabb_t& bounds,
                                                 const std::vector<chlm::float2>& polygon_points) noexcept
        {
            if (polygon_points.size() < 3u)
                return false;

            const chlm::float2 center{ bounds.center() };
            const chlm::float2 extents{ bounds.extents() };
            for (const chlm::float2 axis : polygon_axes(polygon_points))
            {
                const projection_interval_t aabb_projection{ project_aabb(center, extents, axis) };
                const projection_interval_t polygon_projection{ project_polygon(polygon_points, axis) };
                if (!interval_overlaps_strictly(aabb_projection.min,
                                                aabb_projection.max,
                                                polygon_projection.min,
                                                polygon_projection.max))
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] raycast_hit_t make_polygon_raycast_hit(const collision_hit_ref_t& hit_ref,
                                                             const chlm::float2 origin,
                                                             const chlm::float2 delta,
                                                             const float fraction,
                                                             const chlm::float2 normal) noexcept
        {
            return raycast_hit_t{
                .hit = hit_ref,
                .position = origin + (delta * fraction),
                .normal = normal,
                .fraction = fraction,
                .distance = vector_length(delta) * fraction,
                .started_overlapping = false
            };
        }

        [[nodiscard]] std::optional<raycast_hit_t> raycast_against_polygon(
            const collision_hit_ref_t& hit_ref,
            const chlm::float2 origin,
            const chlm::float2 delta) noexcept
        {
            if (!hit_ref.collider || hit_ref.collider->polygon_points.size() < 3u)
                return std::nullopt;

            if (point_in_polygon(origin, hit_ref.collider->polygon_points))
            {
                return raycast_hit_t{
                    .hit = hit_ref,
                    .position = origin,
                    .normal = chlm::float2{ 0.f, 0.f },
                    .fraction = 0.f,
                    .distance = 0.f,
                    .started_overlapping = true
                };
            }

            const collision_aabb_t point_bounds{ collision_aabb_t::from_center_extents(origin, chlm::float2{ 0.f, 0.f }) };
            const chlm::float2 center{ point_bounds.center() };
            const chlm::float2 extents{ point_bounds.extents() };

            float t_entry{ 0.f };
            float t_exit{ 1.f };
            chlm::float2 hit_normal{ 0.f, 0.f };

            for (const chlm::float2 axis : polygon_axes(hit_ref.collider->polygon_points))
            {
                const projection_interval_t moving_projection{ project_aabb(center, extents, axis) };
                const projection_interval_t static_projection{ project_polygon(hit_ref.collider->polygon_points, axis) };
                const float velocity{ (delta.x * axis.x) + (delta.y * axis.y) };

                if (std::fabs(velocity) <= 1.0e-6f)
                {
                    if (!interval_overlaps_strictly(moving_projection.min,
                                                    moving_projection.max,
                                                    static_projection.min,
                                                    static_projection.max))
                    {
                        return std::nullopt;
                    }
                    continue;
                }

                const float t1{ (static_projection.min - moving_projection.max) / velocity };
                const float t2{ (static_projection.max - moving_projection.min) / velocity };
                const float axis_entry{ std::min(t1, t2) };
                const float axis_exit{ std::max(t1, t2) };

                if (axis_entry > t_entry)
                {
                    t_entry = axis_entry;
                    hit_normal = velocity > 0.f ? chlm::float2{ -axis.x, -axis.y } : axis;
                }
                t_exit = std::min(t_exit, axis_exit);
                if (t_entry > t_exit)
                    return std::nullopt;
            }

            if (t_entry < 0.f || t_entry > 1.f)
                return std::nullopt;

            return make_polygon_raycast_hit(hit_ref, origin, delta, t_entry, hit_normal);
        }

        [[nodiscard]] std::optional<sweep_hit_t> sweep_aabb_against_polygon(
            const collision_hit_ref_t& hit_ref,
            const collision_aabb_t& moving_bounds,
            const chlm::float2 delta) noexcept
        {
            if (!hit_ref.collider || hit_ref.collider->polygon_points.size() < 3u)
                return std::nullopt;

            if (aabb_overlaps_polygon(moving_bounds, hit_ref.collider->polygon_points))
            {
                return sweep_hit_t{
                    .hit = hit_ref,
                    .position = moving_bounds.center(),
                    .normal = chlm::float2{ 0.f, 0.f },
                    .fraction = 0.f,
                    .distance = 0.f,
                    .started_overlapping = true
                };
            }

            const chlm::float2 center{ moving_bounds.center() };
            const chlm::float2 extents{ moving_bounds.extents() };
            const float sweep_length{ vector_length(delta) };

            float t_entry{ 0.f };
            float t_exit{ 1.f };
            chlm::float2 hit_normal{ 0.f, 0.f };

            for (const chlm::float2 axis : polygon_axes(hit_ref.collider->polygon_points))
            {
                const projection_interval_t moving_projection{ project_aabb(center, extents, axis) };
                const projection_interval_t static_projection{ project_polygon(hit_ref.collider->polygon_points, axis) };
                const float velocity{ (delta.x * axis.x) + (delta.y * axis.y) };

                if (std::fabs(velocity) <= 1.0e-6f)
                {
                    if (!interval_overlaps_strictly(moving_projection.min,
                                                    moving_projection.max,
                                                    static_projection.min,
                                                    static_projection.max))
                    {
                        return std::nullopt;
                    }
                    continue;
                }

                const float t1{ (static_projection.min - moving_projection.max) / velocity };
                const float t2{ (static_projection.max - moving_projection.min) / velocity };
                const float axis_entry{ std::min(t1, t2) };
                const float axis_exit{ std::max(t1, t2) };

                if (axis_entry > t_entry)
                {
                    t_entry = axis_entry;
                    hit_normal = velocity > 0.f ? chlm::float2{ -axis.x, -axis.y } : axis;
                }
                t_exit = std::min(t_exit, axis_exit);
                if (t_entry > t_exit)
                    return std::nullopt;
            }

            if (t_entry < 0.f || t_entry > 1.f)
                return std::nullopt;

            return sweep_hit_t{
                .hit = hit_ref,
                .position = center + (delta * t_entry),
                .normal = hit_normal,
                .fraction = t_entry,
                .distance = sweep_length * t_entry,
                .started_overlapping = false
            };
        }

        [[nodiscard]] bool collision_aabb_strictly_contains_point(const collision_aabb_t& bounds,
                                                                  const chlm::float2 point) noexcept
        {
            return point.x > bounds.min.x && point.x < bounds.max.x && point.y > bounds.min.y && point.y < bounds.max.y;
        }

        [[nodiscard]] collision_hit_ref_t make_hit_ref(const static_collider_t& collider) noexcept
        {
            return collision_hit_ref_t{
                .collider = &collider,
                .tile_field = nullptr,
                .tile_x = 0,
                .tile_y = 0,
                .bounds = collider.bounds,
                .layer = collider.layer,
                .mask = collider.mask
            };
        }

        [[nodiscard]] collision_hit_ref_t make_hit_ref(const tile_collision_field_t& field,
                                                       const uint32_t x,
                                                       const uint32_t y,
                                                       const tile_collision_cell_t& cell) noexcept
        {
            return collision_hit_ref_t{
                .collider = nullptr,
                .tile_field = &field,
                .tile_x = x,
                .tile_y = y,
                .bounds = field.cell_bounds(x, y),
                .layer = cell.layer,
                .mask = cell.mask
            };
        }

        [[nodiscard]] ray_vs_aabb_result_t raycast_against_aabb(const chlm::float2 origin,
                                                                const chlm::float2 delta,
                                                                const collision_aabb_t& bounds) noexcept
        {
            if (collision_aabb_strictly_contains_point(bounds, origin))
                return ray_vs_aabb_result_t{ .hit = true, .started_overlapping = true };

            float t_min{ 0.f };
            float t_max{ 1.f };
            chlm::float2 hit_normal{ 0.f, 0.f };

            const auto update_axis = [&](const float origin_axis,
                                         const float delta_axis,
                                         const float min_axis,
                                         const float max_axis,
                                         const chlm::float2 negative_normal,
                                         const chlm::float2 positive_normal) -> bool
            {
                constexpr float epsilon{ 1.0e-6f };

                if (std::fabs(delta_axis) <= epsilon)
                    return origin_axis >= min_axis && origin_axis <= max_axis;

                const float inverse_delta{ 1.f / delta_axis };
                float t1{ (min_axis - origin_axis) * inverse_delta };
                float t2{ (max_axis - origin_axis) * inverse_delta };
                chlm::float2 axis_normal{ negative_normal };

                if (t1 > t2)
                {
                    std::swap(t1, t2);
                    axis_normal = positive_normal;
                }

                if (t1 > t_min)
                {
                    t_min = t1;
                    hit_normal = axis_normal;
                }

                t_max = std::min(t_max, t2);
                return t_min <= t_max;
            };

            if (!update_axis(origin.x, delta.x, bounds.min.x, bounds.max.x, chlm::float2{ -1.f, 0.f }, chlm::float2{ 1.f, 0.f }))
                return {};

            if (!update_axis(origin.y, delta.y, bounds.min.y, bounds.max.y, chlm::float2{ 0.f, -1.f }, chlm::float2{ 0.f, 1.f }))
                return {};

            if (t_min < 0.f || t_min > 1.f)
                return {};

            return ray_vs_aabb_result_t{
                .hit = true,
                .started_overlapping = false,
                .fraction = t_min,
                .normal = hit_normal
            };
        }
    } // namespace

    tile_collision_field_t::tile_collision_field_t(const uint32_t width,
                                                   const uint32_t height,
                                                   const chlm::float2 origin,
                                                   const chlm::float2 cell_size)
        : _width{ width }
        , _height{ height }
        , _origin{ origin }
        , _cell_size{ cell_size }
        , _cells(static_cast<size_t>(width) * static_cast<size_t>(height))
    {
    }

    void tile_collision_field_t::fill(const tile_collision_cell_t cell)
    {
        std::fill(_cells.begin(), _cells.end(), cell);
    }

    bool tile_collision_field_t::contains_cell(const uint32_t x, const uint32_t y) const noexcept
    {
        return x < _width && y < _height;
    }

    tile_collision_cell_t* tile_collision_field_t::cell_at(const uint32_t x, const uint32_t y) noexcept
    {
        if (!contains_cell(x, y))
            return nullptr;

        return &_cells[index_of(x, y)];
    }

    const tile_collision_cell_t* tile_collision_field_t::cell_at(const uint32_t x, const uint32_t y) const noexcept
    {
        if (!contains_cell(x, y))
            return nullptr;

        return &_cells[index_of(x, y)];
    }

    collision_aabb_t tile_collision_field_t::cell_bounds(const uint32_t x, const uint32_t y) const noexcept
    {
        const chlm::float2 cell_min{
            _origin.x + (_cell_size.x * static_cast<float>(x)),
            _origin.y + (_cell_size.y * static_cast<float>(y))
        };

        return collision_aabb_t::from_min_size(cell_min, _cell_size);
    }

    size_t tile_collision_field_t::index_of(const uint32_t x, const uint32_t y) const noexcept
    {
        return static_cast<size_t>(y) * static_cast<size_t>(_width) + static_cast<size_t>(x);
    }

    void collision_world_t::clear() noexcept
    {
        _next_static_collider_id = 1;
        _next_tile_field_id = 1;
        _static_colliders.clear();
        _tile_fields.clear();
    }

    const static_collider_t& collision_world_t::add_static_collider(static_collider_t collider)
    {
        if (collider.is_convex_polygon())
            collider.bounds = bounds_for_points(collider.polygon_points);
        collider.id = _next_static_collider_id++;
        return _static_colliders.emplace_back(std::move(collider));
    }

    tile_collision_field_t& collision_world_t::create_tile_collision_field(const uint32_t width,
                                                                           const uint32_t height,
                                                                           const chlm::float2 origin,
                                                                           const chlm::float2 cell_size)
    {
        tile_collision_field_t& field{ _tile_fields.emplace_back(width, height, origin, cell_size) };
        field.set_id(_next_tile_field_id++);
        return field;
    }

    std::vector<collision_hit_ref_t> collision_world_t::point_query(const chlm::float2 point,
                                                                    const collision_query_filter_t filter) const
    {
        std::vector<collision_hit_ref_t> hits;

        for (const static_collider_t& collider : _static_colliders)
        {
            if (!collision_layers_match(collider.layer, collider.mask, filter))
                continue;

            if (collider.is_convex_polygon())
            {
                if (point_in_polygon(point, collider.polygon_points))
                    hits.push_back(make_hit_ref(collider));
            }
            else if (collision_aabb_contains_point(collider.bounds, point))
                hits.push_back(make_hit_ref(collider));
        }

        for (const tile_collision_field_t& field : _tile_fields)
        {
            for (uint32_t y{ 0 }; y < field.height(); ++y)
            {
                for (uint32_t x{ 0 }; x < field.width(); ++x)
                {
                    const tile_collision_cell_t* cell{ field.cell_at(x, y) };
                    if (!cell || !cell->solid || !collision_layers_match(cell->layer, cell->mask, filter))
                        continue;

                    const collision_aabb_t bounds{ field.cell_bounds(x, y) };
                    if (collision_aabb_contains_point(bounds, point))
                        hits.push_back(make_hit_ref(field, x, y, *cell));
                }
            }
        }

        return hits;
    }

    std::vector<collision_hit_ref_t> collision_world_t::overlap_query(const collision_aabb_t& bounds,
                                                                      const collision_query_filter_t filter) const
    {
        std::vector<collision_hit_ref_t> hits;

        for (const static_collider_t& collider : _static_colliders)
        {
            if (!collision_layers_match(collider.layer, collider.mask, filter))
                continue;

            if ((collider.is_convex_polygon() && aabb_overlaps_polygon(bounds, collider.polygon_points)) ||
                (collider.is_aabb() && collision_aabb_overlaps(collider.bounds, bounds)))
            {
                hits.push_back(make_hit_ref(collider));
            }
        }

        for (const tile_collision_field_t& field : _tile_fields)
        {
            for (uint32_t y{ 0 }; y < field.height(); ++y)
            {
                for (uint32_t x{ 0 }; x < field.width(); ++x)
                {
                    const tile_collision_cell_t* cell{ field.cell_at(x, y) };
                    if (!cell || !cell->solid || !collision_layers_match(cell->layer, cell->mask, filter))
                        continue;

                    const collision_aabb_t cell_bounds{ field.cell_bounds(x, y) };
                    if (collision_aabb_overlaps(cell_bounds, bounds))
                        hits.push_back(make_hit_ref(field, x, y, *cell));
                }
            }
        }

        return hits;
    }

    std::optional<raycast_hit_t> collision_world_t::raycast(const chlm::float2 origin,
                                                            const chlm::float2 delta,
                                                            const collision_query_filter_t filter) const
    {
        std::optional<raycast_hit_t> nearest_hit;
        const float ray_length{ vector_length(delta) };

        const auto consider_hit = [&](const collision_hit_ref_t& hit_ref) {
            std::optional<raycast_hit_t> hit;
            if (hit_ref.collider && hit_ref.collider->is_convex_polygon())
                hit = raycast_against_polygon(hit_ref, origin, delta);
            else
            {
                const ray_vs_aabb_result_t result{ raycast_against_aabb(origin, delta, hit_ref.bounds) };
                if (!result.hit)
                    return;

                hit = raycast_hit_t{
                    .hit = hit_ref,
                    .position = origin + (delta * result.fraction),
                    .normal = result.normal,
                    .fraction = result.fraction,
                    .distance = ray_length * result.fraction,
                    .started_overlapping = result.started_overlapping
                };
            }

            if (!hit)
                return;

            if (!nearest_hit
                || hit->fraction < nearest_hit->fraction
                || (nearly_equal_fraction(hit->fraction, nearest_hit->fraction)
                    && hit->started_overlapping
                    && !nearest_hit->started_overlapping))
            {
                nearest_hit = *hit;
            }
        };

        for (const static_collider_t& collider : _static_colliders)
        {
            if (!collision_layers_match(collider.layer, collider.mask, filter))
                continue;

            consider_hit(make_hit_ref(collider));
        }

        for (const tile_collision_field_t& field : _tile_fields)
        {
            for (uint32_t y{ 0 }; y < field.height(); ++y)
            {
                for (uint32_t x{ 0 }; x < field.width(); ++x)
                {
                    const tile_collision_cell_t* cell{ field.cell_at(x, y) };
                    if (!cell || !cell->solid || !collision_layers_match(cell->layer, cell->mask, filter))
                        continue;

                    consider_hit(make_hit_ref(field, x, y, *cell));
                }
            }
        }

        return nearest_hit;
    }

    std::optional<sweep_hit_t> collision_world_t::sweep_aabb(const collision_aabb_t& moving_bounds,
                                                             const chlm::float2 delta,
                                                             const collision_query_filter_t filter) const
    {
        std::optional<sweep_hit_t> nearest_hit;
        const chlm::float2 center{ moving_bounds.center() };
        const chlm::float2 extents{ moving_bounds.extents() };
        const float sweep_length{ vector_length(delta) };

        const auto consider_hit = [&](const collision_hit_ref_t& hit_ref) {
            if (hit_ref.collider && hit_ref.collider->is_convex_polygon())
            {
                const auto hit{ sweep_aabb_against_polygon(hit_ref, moving_bounds, delta) };
                if (!hit)
                    return;
                if (!nearest_hit
                    || hit->fraction < nearest_hit->fraction
                    || (nearly_equal_fraction(hit->fraction, nearest_hit->fraction)
                        && hit->started_overlapping
                        && !nearest_hit->started_overlapping))
                {
                    nearest_hit = *hit;
                }
                return;
            }

            if (collision_aabb_overlaps(moving_bounds, hit_ref.bounds))
            {
                sweep_hit_t hit{
                    .hit = hit_ref,
                    .position = center,
                    .normal = chlm::float2{ 0.f, 0.f },
                    .fraction = 0.f,
                    .distance = 0.f,
                    .started_overlapping = true
                };

                if (!nearest_hit || !nearest_hit->started_overlapping)
                    nearest_hit = hit;

                return;
            }

            const collision_aabb_t expanded_target{
                .min = hit_ref.bounds.min - extents,
                .max = hit_ref.bounds.max + extents
            };
            const ray_vs_aabb_result_t result{ raycast_against_aabb(center, delta, expanded_target) };
            if (!result.hit)
                return;

            if (!result.started_overlapping && result.fraction <= 0.f)
            {
                const float normal_dot_delta{
                    (result.normal.x * delta.x) + (result.normal.y * delta.y)
                };
                if (normal_dot_delta >= 0.f)
                    return;
            }

            sweep_hit_t hit{
                .hit = hit_ref,
                .position = center + (delta * result.fraction),
                .normal = result.normal,
                .fraction = result.fraction,
                .distance = sweep_length * result.fraction,
                .started_overlapping = result.started_overlapping
            };

            if (!nearest_hit
                || hit.fraction < nearest_hit->fraction
                || (nearly_equal_fraction(hit.fraction, nearest_hit->fraction)
                    && hit.started_overlapping
                    && !nearest_hit->started_overlapping))
            {
                nearest_hit = hit;
            }
        };

        for (const static_collider_t& collider : _static_colliders)
        {
            if (!collision_layers_match(collider.layer, collider.mask, filter))
                continue;

            consider_hit(make_hit_ref(collider));
        }

        for (const tile_collision_field_t& field : _tile_fields)
        {
            for (uint32_t y{ 0 }; y < field.height(); ++y)
            {
                for (uint32_t x{ 0 }; x < field.width(); ++x)
                {
                    const tile_collision_cell_t* cell{ field.cell_at(x, y) };
                    if (!cell || !cell->solid || !collision_layers_match(cell->layer, cell->mask, filter))
                        continue;

                    consider_hit(make_hit_ref(field, x, y, *cell));
                }
            }
        }

        return nearest_hit;
    }

    bool collision_layers_match(const collision_layer_t candidate_layer,
                                const collision_mask_t candidate_mask,
                                const collision_query_filter_t filter) noexcept
    {
        return (candidate_layer & filter.mask) != 0u && (candidate_mask & filter.layer) != 0u;
    }

    bool collision_aabb_contains_point(const collision_aabb_t& bounds, const chlm::float2 point) noexcept
    {
        return point.x >= bounds.min.x && point.x <= bounds.max.x && point.y >= bounds.min.y && point.y <= bounds.max.y;
    }

    bool collision_aabb_overlaps(const collision_aabb_t& a, const collision_aabb_t& b) noexcept
    {
        return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y;
    }
} // namespace carrot::collision
