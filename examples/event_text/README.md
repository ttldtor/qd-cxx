# Print an event through std::cout

[Русская версия](README.ru.md). English is authoritative.

This complete public API example creates one owned Quote, selects explicit UTC and prints the returned standard UTF-8 string. It does not initialize timezone data, read files, connect, or change console encoding. Missing business prices remain NaN. It uses EventTextLimits{1024,4096,16384,65536} and rejects malformed UTF-16.

Enable ORG_TTLDTOR_QDCXX_BUILD_EXAMPLES, build qdcxx_event_text_example, and run qdcxx-event-text-example --run. No arguments and --help display the same input-free English help; --help-ru displays Russian. Exit0 is success/help, exit1 formatting/output failure, exit2 invalid arguments.

For a separate installed application, find_package(qdcxx CONFIG REQUIRED), link qdcxx_event_text, and compile [main.cpp](main.cpp). The normal package needs no backend for this UTC example. See the [guide](../../docs/EVENT-TEXT.md) for named contexts and the [contract](../../spec/events/EVENT-TEXT.md) for ownership/budgets. The returned string owns its bytes and can be logged after the event/context is destroyed.
