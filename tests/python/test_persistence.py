import json
import math
from pathlib import Path

import pandas as pd
import pytest
from backtester_data.persistence import (
    write_canonical_data,
    write_metadata,
    write_raw_data,
)


def _make_raw_data() -> pd.DataFrame:
    columns = pd.MultiIndex.from_product(
        [
            ["SPY"],
            [
                "Open",
                "High",
                "Low",
                "Close",
                "Adj Close",
                "Volume",
                "Dividends",
                "Stock Splits",
            ],
        ]
    )

    return pd.DataFrame(
        [
            [
                100.0,
                105.0,
                99.0,
                103.0,
                102.5,
                1_000_000,
                0.0,
                0.0,
            ],
            [
                103.0,
                106.0,
                102.0,
                105.0,
                104.5,
                1_100_000,
                0.5,
                0.0,
            ],
        ],
        index=pd.DatetimeIndex(
            [
                "2026-08-24",
                "2026-08-25",
            ],
            name="Date",
        ),
        columns=columns,
    )


def _make_canonical_data() -> pd.DataFrame:
    return pd.DataFrame(
        {
            "session_date": [
                "2026-08-24",
                "2026-08-25",
            ],
            "symbol": [
                "SPY",
                "SPY",
            ],
            "open": [
                100.0,
                103.0,
            ],
            "high": [
                105.0,
                106.0,
            ],
            "low": [
                99.0,
                102.0,
            ],
            "close": [
                103.0,
                105.0,
            ],
            "volume": [
                1_000_000,
                1_100_000,
            ],
        }
    )


def _make_metadata() -> dict[str, object]:
    return {
        "provider": "yahoo_finance",
        "library": {
            "name": "yfinance",
            "version": "0.2.test",
        },
        "downloaded_at_utc": "2026-09-13T12:00:00+00:00",
        "request": {
            "symbols": ["SPY"],
            "selected_symbol": "SPY",
            "start_date": "2026-08-24",
            "end_date": "2026-08-26",
            "interval": "1d",
        },
        "result": {
            "raw_row_count": 2,
        },
    }


# ---------------------------------------------------------------------------
# Raw data persistence
# ---------------------------------------------------------------------------


def test_write_raw_data_creates_parent_directories(
    tmp_path: Path,
) -> None:
    path = tmp_path / "nested" / "raw" / "spy.csv"

    write_raw_data(
        _make_raw_data(),
        path,
    )

    assert path.exists()


def test_write_raw_data_preserves_provider_structure(
    tmp_path: Path,
) -> None:
    data = _make_raw_data()
    path = tmp_path / "spy.csv"

    write_raw_data(
        data,
        path,
    )

    loaded = pd.read_csv(
        path,
        header=[0, 1],
        index_col=0,
        parse_dates=True,
    )

    loaded.index.name = "Date"

    pd.testing.assert_frame_equal(
        loaded,
        data,
        check_freq=False,
    )


def test_write_raw_data_preserves_missing_values(
    tmp_path: Path,
) -> None:
    data = _make_raw_data()
    data.loc[
        pd.Timestamp("2026-08-25"),
        ("SPY", "Adj Close"),
    ] = math.nan

    path = tmp_path / "spy.csv"

    write_raw_data(
        data,
        path,
    )

    loaded = pd.read_csv(
        path,
        header=[0, 1],
        index_col=0,
        parse_dates=True,
    )

    assert pd.isna(
        loaded.loc[
            pd.Timestamp("2026-08-25"),
            ("SPY", "Adj Close"),
        ]
    )


def test_write_raw_data_does_not_mutate_input(
    tmp_path: Path,
) -> None:
    data = _make_raw_data()
    expected = data.copy(deep=True)

    write_raw_data(
        data,
        tmp_path / "spy.csv",
    )

    pd.testing.assert_frame_equal(
        data,
        expected,
    )


def test_write_raw_data_rejects_overwrite(
    tmp_path: Path,
) -> None:
    path = tmp_path / "spy.csv"
    path.write_text(
        "existing content\n",
        encoding="utf-8",
    )

    with pytest.raises(FileExistsError):
        write_raw_data(
            _make_raw_data(),
            path,
        )

    assert path.read_text(
        encoding="utf-8"
    ) == "existing content\n"


# ---------------------------------------------------------------------------
# Metadata persistence
# ---------------------------------------------------------------------------


def test_write_metadata_creates_parent_directories(
    tmp_path: Path,
) -> None:
    path = (
        tmp_path
        / "nested"
        / "raw"
        / "spy.metadata.json"
    )

    write_metadata(
        _make_metadata(),
        path,
    )

    assert path.exists()


def test_write_metadata_preserves_content(
    tmp_path: Path,
) -> None:
    metadata = _make_metadata()
    path = tmp_path / "metadata.json"

    write_metadata(
        metadata,
        path,
    )

    with path.open(
        "r",
        encoding="utf-8",
    ) as file:
        loaded = json.load(file)

    assert loaded == metadata


def test_write_metadata_sorts_keys(
    tmp_path: Path,
) -> None:
    metadata = {
        "zebra": 1,
        "alpha": 2,
        "middle": 3,
    }

    path = tmp_path / "metadata.json"

    write_metadata(
        metadata,
        path,
    )

    content = path.read_text(
        encoding="utf-8"
    )

    assert content.index('"alpha"') < content.index('"middle"')
    assert content.index('"middle"') < content.index('"zebra"')


def test_write_metadata_ends_with_newline(
    tmp_path: Path,
) -> None:
    path = tmp_path / "metadata.json"

    write_metadata(
        _make_metadata(),
        path,
    )

    content = path.read_text(
        encoding="utf-8"
    )

    assert content.endswith("\n")


def test_write_metadata_rejects_overwrite(
    tmp_path: Path,
) -> None:
    path = tmp_path / "metadata.json"
    path.write_text(
        "existing content\n",
        encoding="utf-8",
    )

    with pytest.raises(FileExistsError):
        write_metadata(
            _make_metadata(),
            path,
        )

    assert path.read_text(
        encoding="utf-8"
    ) == "existing content\n"


# ---------------------------------------------------------------------------
# Canonical persistence
# ---------------------------------------------------------------------------


def test_write_canonical_data_creates_parent_directories(
    tmp_path: Path,
) -> None:
    path = (
        tmp_path
        / "nested"
        / "canonical"
        / "spy.csv"
    )

    write_canonical_data(
        _make_canonical_data(),
        path,
    )

    assert path.exists()


def test_write_canonical_data_writes_exact_schema_without_index(
    tmp_path: Path,
) -> None:
    path = tmp_path / "spy.csv"

    write_canonical_data(
        _make_canonical_data(),
        path,
    )

    lines = path.read_text(
        encoding="utf-8"
    ).splitlines()

    assert lines[0] == (
        "session_date,symbol,open,high,low,close,volume"
    )

    assert lines[1] == (
        "2026-08-24,SPY,100.0,105.0,99.0,103.0,1000000"
    )

    assert lines[2] == (
        "2026-08-25,SPY,103.0,106.0,102.0,105.0,1100000"
    )


def test_write_canonical_data_serializes_integral_float_volume_as_integer(
    tmp_path: Path,
) -> None:
    data = _make_canonical_data()
    data["volume"] = data["volume"].astype(float)

    path = tmp_path / "spy.csv"

    write_canonical_data(
        data,
        path,
    )

    loaded = pd.read_csv(path)

    assert loaded["volume"].tolist() == [
        1_000_000,
        1_100_000,
    ]

    content = path.read_text(
        encoding="utf-8"
    )

    assert "1000000.0" not in content
    assert "1100000.0" not in content


def test_write_canonical_data_does_not_mutate_input(
    tmp_path: Path,
) -> None:
    data = _make_canonical_data()
    data["volume"] = data["volume"].astype(float)

    expected = data.copy(deep=True)

    write_canonical_data(
        data,
        tmp_path / "spy.csv",
    )

    pd.testing.assert_frame_equal(
        data,
        expected,
    )


def test_write_canonical_data_requires_volume_column(
    tmp_path: Path,
) -> None:
    data = _make_canonical_data().drop(
        columns=["volume"]
    )

    with pytest.raises(
        ValueError,
        match="Canonical dataset must contain a volume column",
    ):
        write_canonical_data(
            data,
            tmp_path / "spy.csv",
        )


def test_write_canonical_data_requires_numeric_volume(
    tmp_path: Path,
) -> None:
    data = _make_canonical_data()

    data["volume"] = [
        "1000000",
        "1100000",
    ]

    with pytest.raises(
        ValueError,
        match="Canonical volume must be numeric before serialization",
    ):
        write_canonical_data(
            data,
            tmp_path / "spy.csv",
        )


def test_write_canonical_data_rejects_missing_volume(
    tmp_path: Path,
) -> None:
    data = _make_canonical_data()
    data["volume"] = data["volume"].astype(float)
    data.loc[0, "volume"] = math.nan

    with pytest.raises(
        ValueError,
        match=(
            "Canonical volume cannot contain missing values "
            "before serialization"
        ),
    ):
        write_canonical_data(
            data,
            tmp_path / "spy.csv",
        )


def test_write_canonical_data_rejects_non_finite_volume(
    tmp_path: Path,
) -> None:
    data = _make_canonical_data()
    data["volume"] = data["volume"].astype(float)
    data.loc[0, "volume"] = math.inf

    with pytest.raises(
        ValueError,
        match=(
            "Canonical volume must contain only finite values "
            "before serialization"
        ),
    ):
        write_canonical_data(
            data,
            tmp_path / "spy.csv",
        )


def test_write_canonical_data_rejects_negative_volume(
    tmp_path: Path,
) -> None:
    data = _make_canonical_data()
    data.loc[0, "volume"] = -1

    with pytest.raises(
        ValueError,
        match=(
            "Canonical volume cannot be negative "
            "before serialization"
        ),
    ):
        write_canonical_data(
            data,
            tmp_path / "spy.csv",
        )


def test_write_canonical_data_rejects_fractional_volume(
    tmp_path: Path,
) -> None:
    data = _make_canonical_data()
    data["volume"] = data["volume"].astype(float)
    data.loc[0, "volume"] = 1000.5

    with pytest.raises(
        ValueError,
        match=(
            "Canonical volume must contain integer values "
            "before serialization"
        ),
    ):
        write_canonical_data(
            data,
            tmp_path / "spy.csv",
        )


def test_write_canonical_data_accepts_zero_volume(
    tmp_path: Path,
) -> None:
    data = _make_canonical_data()
    data.loc[0, "volume"] = 0

    path = tmp_path / "spy.csv"

    write_canonical_data(
        data,
        path,
    )

    loaded = pd.read_csv(path)

    assert loaded.loc[0, "volume"] == 0


def test_write_canonical_data_rejects_overwrite(
    tmp_path: Path,
) -> None:
    path = tmp_path / "spy.csv"
    path.write_text(
        "existing content\n",
        encoding="utf-8",
    )

    with pytest.raises(FileExistsError):
        write_canonical_data(
            _make_canonical_data(),
            path,
        )

    assert path.read_text(
        encoding="utf-8"
    ) == "existing content\n"