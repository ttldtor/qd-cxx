# Refresh QTP Basic credentials

[Русская версия](README.ru.md) · [Feed authentication](https://github.com/ttldtor/qd-cxx/blob/master/docs/FEED-API.md#qtp-basic-authorization)

The [example](main.cpp) connects to a QTP server over verified TLS. Once
the server accepts the initial credentials, the example updates its
`CredentialProvider`, calls `reconnect()` and waits for the server to
accept the replacement credentials on a new connection. It prints the two
source epoch numbers. The application supplies both Basic values. OAuth
token acquisition and automatic renewal are outside this API.

Build with the qualified compiler and a [TLS-enabled qd-cxx package](https://github.com/ttldtor/qd-cxx/blob/master/docs/FEED-API.md#connect-with-verified-tls).
Use the same qualified OpenSSL installation as the package.

```sh
cmake -S examples/feed_token_refresh -B build/feed-token-refresh -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qdcxx -DORG_TTLDTOR_QDCXX_OPENSSL_ROOT=/path/to/qualified-openssl
cmake --build build/feed-token-refresh
```

Pass the QTP TLS route, expected certificate identity and PEM trust file.
Pipe two complete `Basic <base64>` lines from your credential source into
stdin in initial, replacement order. For example, on a POSIX shell with an
already protected input file, use this command.

```sh
build/feed-token-refresh/qdcxx-feed-token-refresh qtp.example.net:7443 qtp.example.net trust.pem < credentials.txt
```

Keep credentials out of command-line arguments and logs. The sample does
not guarantee secure erasure from memory. Use a QTP server configured to
challenge for Basic and accept the two authorizations. On terminal
rejection, retry with a new endpoint. The sample's TLS resource limits are
for its standalone process. For embedding, see [TLS resource declarations](https://github.com/ttldtor/qd-cxx/blob/master/spec/transport/TLS-RESOURCES.md).
