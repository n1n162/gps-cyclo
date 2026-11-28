#!/usr/bin/env python3
"""Test unitaire de l'assemblage SQLite -> GVT1 du convertisseur PBF."""

import importlib.util
import os
import sqlite3
import struct
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(ROOT, "tools", "osm_pbf_to_gvt.py")
SPEC = importlib.util.spec_from_file_location("osm_pbf_to_gvt", SCRIPT)
CONVERTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CONVERTER)


def main():
    with tempfile.TemporaryDirectory(prefix="test_pbf_gvt_") as tmp:
        db_path = os.path.join(tmp, "records.sqlite")
        output = os.path.join(tmp, "test.gvt")
        db = sqlite3.connect(db_path)
        try:
            db.execute(
                "CREATE TABLE records (tile_key INTEGER NOT NULL, payload BLOB NOT NULL)"
            )
            key_a = CONVERTER.key_of(12, 5, 7)
            key_b = CONVERTER.key_of(11, 10, 20)
            payload_a = CONVERTER.encode_tile(
                [("L", CONVERTER.MAJOR, [(10, 20), (30, 40)])]
            )[1:]
            payload_b1 = CONVERTER.encode_tile(
                [("L", CONVERTER.TRACK, [(1, 2), (3, 4)])]
            )[1:]
            payload_b2 = CONVERTER.encode_tile(
                [("L", CONVERTER.RIVER, [(5, 6), (7, 8)])]
            )[1:]

            # Insertion volontairement désordonnée ; l'index final doit être trié.
            db.executemany(
                "INSERT INTO records(tile_key, payload) VALUES (?, ?)",
                [
                    (key_a, payload_a),
                    (key_b, payload_b1),
                    (key_b, payload_b2),
                ],
            )
            db.commit()

            result = CONVERTER.write_gvt(
                db, output, (-1.0, -2.0, 3.0, 4.0), 11, 12
            )
        finally:
            db.close()

        with open(output, "rb") as f:
            raw = f.read()

        header = CONVERTER.HEADER.unpack_from(raw, 0)
        assert header[0] == CONVERTER.MAGIC, header
        assert header[1] == CONVERTER.VERSION, header
        assert header[6] == 2, header
        assert header[7] == CONVERTER.HEADER_SIZE, header

        index_start = CONVERTER.HEADER_SIZE
        entries = [
            CONVERTER.INDEX.unpack_from(raw, index_start + i * CONVERTER.INDEX_SIZE)
            for i in range(2)
        ]
        assert [entry[0] for entry in entries] == [key_b, key_a], entries

        expected_blobs = [
            CONVERTER.varint(2) + payload_b1 + payload_b2,
            CONVERTER.varint(1) + payload_a,
        ]
        cursor = CONVERTER.HEADER_SIZE + 2 * CONVERTER.INDEX_SIZE
        for entry, expected in zip(entries, expected_blobs):
            key, offset, size = entry
            assert offset == cursor, (key, offset, cursor)
            assert size == len(expected), (key, size, len(expected))
            assert raw[offset:offset + size] == expected, key
            cursor += size

        assert cursor == len(raw), (cursor, len(raw))
        assert result[0] == 2, result
        assert result[1] == len(raw), result
        assert result[2] == max(map(len, expected_blobs)), result
        assert result[3] == 0, result
        assert result[4] == {
            11: {"tiles": 1, "max_bytes": len(expected_blobs[0]), "over_cache": 0},
            12: {"tiles": 1, "max_bytes": len(expected_blobs[1]), "over_cache": 0},
        }, result

    print("OK assemblage PBF -> GVT1 : en-tête, index trié, offsets, tailles et données")


if __name__ == "__main__":
    main()
