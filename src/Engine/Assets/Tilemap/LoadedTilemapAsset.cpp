//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "LoadedTilemapAsset.h"

namespace carrot::assets {
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
} // namespace carrot::assets
