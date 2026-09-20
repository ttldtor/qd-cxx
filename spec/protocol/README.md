# Protocol specification

This area will describe QTP framing, message boundaries, negotiation, protocol states, error behavior, and the separation between protocol and transport.

The specification should work over abstract byte input/output and must not assume a particular socket or event-loop library. Byte-level dependencies belong in `spec/encoding/`.

Status: boundary inventory pending `QDCXX-004`.
