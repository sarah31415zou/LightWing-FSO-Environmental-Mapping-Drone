# Telemetry analysis

The host GUI records each event as a CSV row. The new offline tools make it
possible to compare several field runs without opening the GUI.

## Record schema

The logger writes these columns:

| Column | Meaning |
| --- | --- |
| `ts` | Unix timestamp in seconds |
| `event` | Event type, including `RX` for one received frame |
| `seq` | Receiver sequence number when available |
| `len` | Payload length in bytes |
| `crc` | `1` for a verified frame and `0` for a rejected frame |
| `ber_ppm` | Receiver bit-error-rate estimate in parts per million |
| `rssi` | Receiver signal-strength proxy |
| `info` | Free-form host-side context |

## Generate a report

From the `securir-comms/host` directory, run:

```bash
python telemetry_report.py path/to/session.csv
```

The text report includes the number of logged samples and received frames, CRC
pass rate, recording duration, BER statistics, and RSSI-proxy range.

Use `--json` for an output format that can be collected by a lab notebook,
a CI artifact, or a plotting script:

```bash
python telemetry_report.py path/to/session.csv --json
```

## Decode raw frame streams

`frame_decoder.py` accepts chunks from a raw byte source and returns only
frames whose preamble, sync word, declared payload length, and CRC16-CCITT
checksum all validate. Its `DecoderStats` counters show how much noise was
discarded and how often a CRC or length failure caused resynchronization.

```python
from frame_decoder import StreamFrameDecoder

decoder = StreamFrameDecoder()
for chunk in raw_byte_chunks:
    for frame in decoder.feed(chunk):
        consume(frame.seq, frame.flags, frame.payload)

print(decoder.stats)
```

The byte stream used here is the protocol frame before Manchester coding. A
physical receiver must first recover and decode Manchester symbols before
passing bytes to the stream decoder.
