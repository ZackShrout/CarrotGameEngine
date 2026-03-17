//
// Created by zshrout on 11/28/25.
// Copyright (c) 2025 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Game.h"

#include <CarrotEngine.h>
#include <filesystem>
#include <optional>

#include "Utils/File/PlatformPaths.h"

[[nodiscard]] static std::optional<std::filesystem::path> find_repo_root(std::filesystem::path start)
{
    start = std::filesystem::weakly_canonical(start);

    while (!start.empty())
    {
        if (std::filesystem::exists(start / ".git") || std::filesystem::exists(start / "CMakeLists.txt"))
            return start;

        const std::filesystem::path parent = start.parent_path();
        if (parent == start)
            break;

        start = parent;
    }

    return std::nullopt;
}

[[nodiscard]] carrot::core::engine_paths_t make_engine_paths()
{
    carrot::core::engine_paths_t paths{};

    const std::filesystem::path exe_dir{ carrot::utils::file::executable_directory() };
    const auto repo_root = find_repo_root(exe_dir);
    if (!repo_root)
        return paths;

    const std::filesystem::path engine_root = *repo_root / "assets";
    const std::filesystem::path game_root   = *repo_root / "Game" / "assets";
    const std::filesystem::path source_root = *repo_root / "Game" / "source";
    const std::filesystem::path save_root   = *repo_root / "Game" / "saved";

    if (std::filesystem::exists(engine_root))
        paths.engine_root = std::filesystem::weakly_canonical(engine_root);

    if (std::filesystem::exists(game_root))
        paths.game_root = std::filesystem::weakly_canonical(game_root);

    if (std::filesystem::exists(source_root))
        paths.source_root = std::filesystem::weakly_canonical(source_root);

    std::filesystem::create_directories(save_root);
    paths.save_root = std::filesystem::weakly_canonical(save_root);

    return paths;
}

int main()
{
    sandbox::sandbox_t* game{ new sandbox::sandbox_t() };

    carrot::engine_t::get().init(make_engine_paths());
    carrot::engine_t::get().run(game);

    delete game;

    return 0;
}
