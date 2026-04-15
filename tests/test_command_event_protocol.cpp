#include <gtest/gtest.h>
#include "CommandEventProtocol.h"

using namespace MoLab;

TEST(CommandEventProtocolTest, ParseCommandJsonLineValidAndInvalid) {
    ParsedCommand command;
    std::string error;

    const std::string valid = R"({"command":"run_tick","request_id":"req-1","payload":{"count":5}})";
    EXPECT_TRUE(parseCommandJsonLine(valid, command, &error));
    EXPECT_EQ(command.command, "run_tick");
    EXPECT_EQ(command.request_id, "req-1");
    EXPECT_EQ(command.payload["count"], 5);

    const std::string invalid = R"({"request_id":"req-2"})";
    EXPECT_FALSE(parseCommandJsonLine(invalid, command, &error));
    EXPECT_FALSE(error.empty());

    const std::string compat = R"({"type":"command","id":"req-3","name":"get_status"})";
    EXPECT_TRUE(parseCommandJsonLine(compat, command, &error));
    EXPECT_EQ(command.command, "get_status");
    EXPECT_EQ(command.request_id, "req-3");

    const std::string missing_request_id = R"({"type":"command","name":"get_status"})";
    EXPECT_FALSE(parseCommandJsonLine(missing_request_id, command, &error));
    EXPECT_FALSE(error.empty());

    const std::string empty_request_id = R"({"type":"command","name":"get_status","id":""})";
    EXPECT_FALSE(parseCommandJsonLine(empty_request_id, command, &error));
    EXPECT_FALSE(error.empty());
}

TEST(CommandEventProtocolTest, BuildAckErrorAndEventJson) {
    const auto ack = buildAckJson("req-1", { {"status", "accepted"} });
    EXPECT_EQ(ack["type"], "ack");
    EXPECT_EQ(ack["ok"], true);
    EXPECT_EQ(ack["request_id"], "req-1");
    EXPECT_EQ(ack["data"]["status"], "accepted");

    const auto error = buildErrorJson("req-2", "invalid_command", "Unsupported command");
    EXPECT_EQ(error["type"], "error");
    EXPECT_EQ(error["ok"], false);
    EXPECT_EQ(error["request_id"], "req-2");
    EXPECT_EQ(error["error"]["code"], "invalid_command");
    EXPECT_EQ(error["error"]["message"], "Unsupported command");

    const auto event = buildEventJson("status", { {"running", true}, {"tick", 42} });
    EXPECT_EQ(event["type"], "event");
    EXPECT_EQ(event["event"], "status");
    EXPECT_EQ(event["payload"]["running"], true);
    EXPECT_EQ(event["payload"]["tick"], 42);

    const auto sequenced_event = buildEventJson("status", { {"tick", 43} }, 7);
    EXPECT_EQ(sequenced_event["event_seq"], 7);
}
