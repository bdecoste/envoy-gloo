licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load(
    "//bazel:envoy_test.bzl",
    "envoy_gloo_cc_test",
)

envoy_package()

envoy_gloo_cc_test(
    name = "client_impl_test",
    srcs = ["client_impl_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/nats/streaming:client_lib",
        "//test/mocks/nats:nats_mocks",
        "//test/mocks/nats/streaming:nats_streaming_mocks",
        "//test/mocks/tcp:tcp_mocks",
        "@envoy//source/common/common:assert_lib",
        "@envoy//test/mocks/event:event_mocks",
        "@envoy//test/mocks/runtime:runtime_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_gloo_cc_test(
    name = "connect_response_handler_test",
    srcs = ["connect_response_handler_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/nats/streaming:connect_response_handler_lib",
        "@envoy//source/common/common:assert_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_gloo_cc_test(
    name = "heartbeat_handler_test",
    srcs = ["heartbeat_handler_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/nats/streaming:heartbeat_handler_lib",
        "//test/mocks/nats:nats_mocks",
        "@envoy//source/common/common:assert_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_gloo_cc_test(
    name = "message_utility_test",
    srcs = ["message_utility_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/nats/streaming:message_utility_lib",
        "@envoy//source/common/common:assert_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_gloo_cc_test(
    name = "pub_request_handler_test",
    srcs = ["pub_request_handler_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/nats:codec_lib",
        "//source/common/nats/streaming:pub_request_handler_lib",
        "//test/mocks/nats/streaming:nats_streaming_mocks",
        "@envoy//source/common/common:assert_lib",
        "@envoy//test/mocks/event:event_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)
