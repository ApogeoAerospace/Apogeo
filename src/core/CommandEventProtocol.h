#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace MoLab {

struct ParsedCommand {
    std::string command;
    std::string request_id;
    nlohmann::json payload = nlohmann::json::object();
};

inline bool parseCommandJsonLine(const std::string& line, ParsedCommand& out, std::string* error = nullptr) {
    try {
        const auto json = nlohmann::json::parse(line);

        if (!json.is_object()) {
            if (error) {
                *error = "Command must be a JSON object";
            }
            return false;
        }

        const bool has_command = json.contains("command") && json["command"].is_string();
        const bool has_name = json.contains("name") && json["name"].is_string();
        if (!has_command && !has_name) {
            if (error) {
                *error = "Missing or invalid 'command' or 'name' field";
            }
            return false;
        }

        out.command = has_command ? json["command"].get<std::string>() : json["name"].get<std::string>();
        out.request_id = json.value("request_id", json.value("id", ""));
        out.payload = json.value("payload", nlohmann::json::object());

        return true;
    } catch (const std::exception& e) {
        if (error) {
            *error = e.what();
        }
        return false;
    }
}

inline nlohmann::json buildAckJson(const std::string& request_id,
                                   const nlohmann::json& data = nlohmann::json::object()) {
    return nlohmann::json{
        {"type", "ack"},
        {"ok", true},
        {"request_id", request_id},
        {"data", data}
    };
}

inline nlohmann::json buildErrorJson(const std::string& request_id,
                                     const std::string& code,
                                     const std::string& message,
                                     const nlohmann::json& details = nlohmann::json::object()) {
    return nlohmann::json{
        {"type", "error"},
        {"ok", false},
        {"request_id", request_id},
        {"error", {
            {"code", code},
            {"message", message},
            {"details", details}
        }}
    };
}

inline nlohmann::json buildEventJson(const std::string& event_name,
                                     const nlohmann::json& payload = nlohmann::json::object(),
                                     uint64_t event_seq = 0) {
    auto event = nlohmann::json{
        {"type", "event"},
        {"event", event_name},
        {"payload", payload}
    };

    if (event_seq > 0) {
        event["event_seq"] = event_seq;
    }

    return event;
}

} // namespace MoLab
