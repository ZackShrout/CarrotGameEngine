//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "LoadedTilemapAsset.h"

namespace carrot::assets {
    void loaded_tilemap_asset_t::build_tile_render_chunks_if_needed() const
    {
        if (_tile_render_chunks_built)
            return;

        _tile_render_chunks_by_layer.clear();
        _tile_render_chunks_by_layer.resize(_tilemap.layers().size());

        for (std::size_t layer_index{ 0u }; layer_index < _tilemap.layers().size(); ++layer_index)
        {
            const tilemap_layer_t& layer{ _tilemap.layers()[layer_index] };
            if (layer.kind != tilemap_layer_kind_t::tile || layer.width == 0u || layer.height == 0u || layer.gids.empty())
                continue;

            auto& chunks{ _tile_render_chunks_by_layer[layer_index] };
            std::unordered_map<std::uint64_t, std::size_t> chunk_lookup;

            for (std::uint32_t row{ 0u }; row < layer.height; ++row)
            {
                for (std::uint32_t col{ 0u }; col < layer.width; ++col)
                {
                    const std::uint32_t cell_index{ row * layer.width + col };
                    if (cell_index >= layer.gids.size() || layer.gids[cell_index] == 0u)
                        continue;

                    const std::uint32_t chunk_x{ col / k_render_chunk_size_tiles };
                    const std::uint32_t chunk_y{ row / k_render_chunk_size_tiles };
                    const std::uint64_t chunk_key{
                        (static_cast<std::uint64_t>(chunk_y) << 32u) | static_cast<std::uint64_t>(chunk_x)
                    };

                    auto it{ chunk_lookup.find(chunk_key) };
                    if (it == chunk_lookup.end())
                    {
                        const std::size_t chunk_index{ chunks.size() };
                        chunk_lookup.emplace(chunk_key, chunk_index);
                        chunks.push_back({
                            .layer_index = static_cast<std::uint32_t>(layer_index),
                            .chunk_x = chunk_x,
                            .chunk_y = chunk_y,
                            .tile_min_x = chunk_x * k_render_chunk_size_tiles,
                            .tile_min_y = chunk_y * k_render_chunk_size_tiles,
                            .tile_max_x = std::min(layer.width, (chunk_x + 1u) * k_render_chunk_size_tiles),
                            .tile_max_y = std::min(layer.height, (chunk_y + 1u) * k_render_chunk_size_tiles),
                            .occupied_cell_indices = { }
                        });
                        it = chunk_lookup.find(chunk_key);
                    }

                    chunks[it->second].occupied_cell_indices.push_back(cell_index);
                }
            }
        }

        _tile_render_chunks_built = true;
    }

    const tilemap_layer_t* loaded_tilemap_asset_t::find_object_layer(const std::string_view name) const noexcept
    {
        for (const tilemap_layer_t& layer : _tilemap.layers())
        {
            if (layer.kind == tilemap_layer_kind_t::object && layer.name == name)
                return &layer;
        }

        return nullptr;
    }

    const tilemap_object_t* loaded_tilemap_asset_t::find_object_by_name(const std::string_view name) const noexcept
    {
        for (const tilemap_layer_t& layer : _tilemap.layers())
        {
            if (layer.kind != tilemap_layer_kind_t::object)
                continue;

            for (const tilemap_object_t& object : layer.objects)
            {
                if (object.name == name)
                    return &object;
            }
        }

        return nullptr;
    }

    const tilemap_object_t* loaded_tilemap_asset_t::find_first_object_by_type(const std::string_view type) const noexcept
    {
        for (const tilemap_layer_t& layer : _tilemap.layers())
        {
            if (layer.kind != tilemap_layer_kind_t::object)
                continue;

            for (const tilemap_object_t& object : layer.objects)
            {
                if (object.type == type)
                    return &object;
            }
        }

        return nullptr;
    }

    std::vector<const tilemap_object_t*> loaded_tilemap_asset_t::find_objects_by_type(const std::string_view type) const
    {
        std::vector<const tilemap_object_t*> matches;

        for (const tilemap_layer_t& layer : _tilemap.layers())
        {
            if (layer.kind != tilemap_layer_kind_t::object)
                continue;

            for (const tilemap_object_t& object : layer.objects)
            {
                if (object.type == type)
                    matches.push_back(&object);
            }
        }

        return matches;
    }

    std::vector<const tilemap_object_t*> loaded_tilemap_asset_t::find_objects_by_type_in_layer(
        const std::string_view layer_name,
        const std::string_view type) const
    {
        std::vector<const tilemap_object_t*> matches;
        const tilemap_layer_t* layer{ find_object_layer(layer_name) };
        if (!layer)
            return matches;

        for (const tilemap_object_t& object : layer->objects)
        {
            if (object.type == type)
                matches.push_back(&object);
        }

        return matches;
    }

    std::span<const tilemap_object_t> loaded_tilemap_asset_t::objects_in_layer(const std::string_view name) const noexcept
    {
        if (const tilemap_layer_t* layer{ find_object_layer(name) })
            return std::span<const tilemap_object_t>{ layer->objects };

        return {};
    }

    std::span<const tilemap_render_chunk_t> loaded_tilemap_asset_t::tile_render_chunks_for_layer(
        const std::size_t layer_index) const noexcept
    {
        build_tile_render_chunks_if_needed();

        if (layer_index >= _tile_render_chunks_by_layer.size())
            return {};

        return std::span<const tilemap_render_chunk_t>{ _tile_render_chunks_by_layer[layer_index] };
    }
} // namespace carrot::assets
