#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {
/** Read the complete bytes of a private test file.
 * @param path File to inspect.
 * @return Owned bytes; empty when absent.
 */
std::string readFile(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);

    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

/** Exercise formatter success/failure without depending on a scripting runtime.
 * @param executable Copied executable with a private mode sidecar.
 * @param version Whether only version reporting was requested.
 * @param source Temporary source selected by the driver.
 * @return Zero on success; a deliberate diagnostic failure otherwise.
 */
int runProbe(const std::filesystem::path &executable, bool version, const std::filesystem::path &source) {
    const bool uncrustify = executable.filename().string().find("uncrustify") != std::string::npos;
    auto modePath = executable;
    modePath += ".mode";
    const auto mode = readFile(modePath);
    auto callsPath = executable;
    callsPath += ".calls";
    std::ofstream(callsPath, std::ios::app) << (version ? "version" : "format") << '\n';

    if (version) {
        std::cout << (uncrustify ? "Uncrustify-" : "clang-format version ")
                  << (mode == "wrong-version" ? "99.0.0"
                      : uncrustify            ? "0.83.0"
                                              : "20.1.8")
                  << '\n';

        return mode == "version-nonzero" ? 37 : 0;
    }

    if (mode == "powershell") {
        return std::system("pwsh -NoProfile -Command exit");
    }

    if (mode == "interrupted") {
        std::raise(SIGTERM);
    }

    if (mode == "delete-output") {
        std::filesystem::remove(source);

        return 0;
    }

    auto content = readFile(source);
    const std::string from = uncrustify ? "BAD" : "U_DONE";
    const std::string to = uncrustify ? "U_DONE" : "GOOD";
    auto position = content.find(from);

    if (position != std::string::npos) {
        content.replace(position, from.size(), to);
    }

    std::ofstream(source, std::ios::binary) << content;

    if (mode == "warning" || mode == "error" || mode == "unknown" || mode == "invalid") {
        std::cerr << mode << ": intentional formatter control\n";
    }

    return mode == "nonzero" ? 37 : 0;
}
} // namespace

/** Native fake formatter entry point with platform-native filename transport.
 * @param argc Number of command arguments.
 * @param argv Executable and formatter arguments.
 * @return Deterministic fake formatter status.
 */
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    return runProbe(argv[0], argc == 2 && std::wstring(argv[1]) == L"--version", argv[argc - 1]);
}
#else
int main(int argc, char **argv) {
    return runProbe(argv[0], argc == 2 && std::string(argv[1]) == "--version", argv[argc - 1]);
}
#endif
