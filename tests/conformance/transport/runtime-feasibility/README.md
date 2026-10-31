# Optional runtime feasibility projects

[Русская версия](README.ru.md). English is authoritative.

These separate CMake projects qualify mechanisms for [ADR-0006](../../../../docs/decisions/ADR-0006-live-runtime.md).
They are not included in the ordinary root build and do not implement production transport.

- [DNS](dns/README.md): pinned explicit-configuration c-ares SDK variant, caller-driven UDP/TCP,
  pending cancellation, bounded receive/resume and configuration rejection. Default OFF.
- [TCP/TLS](tls/README.md): pinned standalone Asio/OpenSSL SDKs, caller-driven IOCP/epoll,
  pending connect/read/write/handshake/shutdown, verified synthetic identities and help. Default OFF.

Acquisition is explicit, local and hash-checked using each project's lock file. Keep original archives
separate from patched sources. No global package manager, installation, environment or firewall changes
are required. The DNS patch and upstream license are tracked; SDK sources/build caches, generated test
keys and raw logs are local outputs. Follow the component README recipes from an owned build directory.
Windows recipes use PowerShell and an MSVC x64 developer environment; native CMake/CTest are used on
both OSes, without a Python dependency. The DNS patch recipe additionally requires `patch`; Git for
Windows provides it in its Unix tools, or apply the reviewed patch with an equivalent native tool.

See [the contract](../CONTRACTS.md) and [validation](../VALIDATION.md) for source facts, executed
qualification, resource scopes and remaining production integration. Passing the prototypes does not
establish a live QTP client, an SDK-wide heap ceiling or an external provider profile.
