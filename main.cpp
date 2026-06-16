#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

#include "solver/case_data.h"
#include "render/window.h"

int main(int argc, char* argv[]) {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::debug);

    if (argc < 2) {
        spdlog::error("Usage: {} <case.json>", argc >= 1 ? argv[0] : "rtfs2d");
        return 1;
    }

    const char* json_path = argv[1];
    spdlog::info("Loading case from: {}", json_path);

    auto stem = std::filesystem::path(json_path).stem().string();

    std::ifstream file(json_path);
    if (!file.is_open()) {
        spdlog::error("Cannot open JSON file: {}", json_path);
        return 1;
    }
    auto j = nlohmann::json::parse(file);
    auto case_data = std::make_unique<rtfs2d::CaseData>(stem);
    try {
        case_data->ParseJson(j);
    } catch (std::exception& e) {
        spdlog::error("Failed to load case file {}: {}", json_path, e.what());
        return 1;
    }
    rtfs2d::Window w(std::move(case_data));
    w.Show();
    return 0;
}
