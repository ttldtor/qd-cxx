# Beast WSS dependency profile

[Русская версия](README.ru.md). English is authoritative.

The standalone profile installs the Boost headers needed by the private WSS build target. The probe compiles and links the Boost.Asio TCP stream, OpenSSL stream and Beast WebSocket stream. It qualifies the dependency graph. TLS session, certificate and WebSocket I/O behavior have separate qualification.

The manifest contains 940 files, including the original Boost Software License 1.0 and version header. The header selection is the union of the MSVC and GCC compiler dependency lists for the [probe](../../tests/conformance/beast-wss-dependency/probe.cpp) under the target's definitions. Extend the selection and repeat both builds when the adapter requires additional headers or definitions. The release, exact archive hash and source inventory hash are recorded in the [profile](boost-1.92.0.json) and [inventory](boost-1.92.0-sources.sha256.tsv).

The target uses header-only Beast and Boost.Asio, with Boost library auto-linking disabled. Windows selects Windows 10 SDK API definitions, avoids Win32 min/max macros and links Winsock. Both platforms link the project-qualified static OpenSSL 3.5.9 SDK selected by the consumer. The SDK is verified before linking. Boost include paths and definitions apply to targets linked against the private package. The QTP targets continue to use their existing standalone Asio source profile. [OpenSSL profile](../tls-profile/openssl-3.5.9.json)

Configure, build, install and test a relocated copy with the commands in the [provisioning guide](../../tools/deps/BEAST-WSS.md).

[The local TLS boundary and qualification](../../spec/transport/BEAST-WSS-TLS.md) cover ownership, certificate verification, bounded backing and cancellation.
