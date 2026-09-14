import json
from datetime import UTC, date, datetime
from pathlib import Path

import backtester_data.pipeline as pipeline_module
import pandas as pd
import pytest
from backtester_data.pipeline import run_daily_data_pipeline


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


def _expected_canonical_data() -> pd.DataFrame:
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


def test_run_daily_data_pipeline_produces_expected_artifacts(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    raw_data = _make_raw_data()

    fixed_time = datetime(
        2026,
        9,
        13,
        20,
        30,
        tzinfo=UTC,
    )

    def fake_download_daily_data(
        symbol: str,
        start_date: date,
        end_date: date,
    ) -> pd.DataFrame:
        assert symbol == "SPY"
        assert start_date == date(2026, 8, 24)
        assert end_date == date(2026, 8, 26)

        return raw_data

    monkeypatch.setattr(
        pipeline_module,
        "download_daily_data",
        fake_download_daily_data,
    )

    monkeypatch.setattr(
        pipeline_module,
        "_utc_now",
        lambda: fixed_time,
    )

    raw_path = tmp_path / "raw" / "spy.csv"
    metadata_path = tmp_path / "raw" / "spy.metadata.json"
    canonical_path = tmp_path / "canonical" / "spy.csv"

    result = run_daily_data_pipeline(
        symbol="SPY",
        start_date=date(2026, 8, 24),
        end_date=date(2026, 8, 26),
        raw_path=raw_path,
        metadata_path=metadata_path,
        canonical_path=canonical_path,
    )

    pd.testing.assert_frame_equal(
        result,
        _expected_canonical_data(),
    )

    assert raw_path.exists()
    assert metadata_path.exists()
    assert canonical_path.exists()

    with metadata_path.open(
        "r",
        encoding="utf-8",
    ) as file:
        metadata = json.load(file)

    assert metadata["provider"] == "yahoo_finance"
    assert metadata["downloaded_at_utc"] == fixed_time.isoformat()
    assert metadata["request"]["selected_symbol"] == "SPY"
    assert metadata["result"]["raw_row_count"] == 2

    canonical_lines = canonical_path.read_text(
        encoding="utf-8"
    ).splitlines()

    assert canonical_lines == [
        "session_date,symbol,open,high,low,close,volume",
        "2026-08-24,SPY,100.0,105.0,99.0,103.0,1000000",
        "2026-08-25,SPY,103.0,106.0,102.0,105.0,1100000",
    ]


def test_run_daily_data_pipeline_preserves_raw_before_provider_validation_failure(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    raw_data = _make_raw_data().iloc[0:0]

    monkeypatch.setattr(
        pipeline_module,
        "download_daily_data",
        lambda symbol, start_date, end_date: raw_data,
    )

    monkeypatch.setattr(
        pipeline_module,
        "_utc_now",
        lambda: datetime(
            2026,
            9,
            13,
            20,
            30,
            tzinfo=UTC,
        ),
    )

    raw_path = tmp_path / "raw" / "spy.csv"
    metadata_path = tmp_path / "raw" / "spy.metadata.json"
    canonical_path = tmp_path / "canonical" / "spy.csv"

    with pytest.raises(
        ValueError,
        match="Provider dataset cannot be empty",
    ):
        run_daily_data_pipeline(
            symbol="SPY",
            start_date=date(2026, 8, 24),
            end_date=date(2026, 8, 26),
            raw_path=raw_path,
            metadata_path=metadata_path,
            canonical_path=canonical_path,
        )

    assert raw_path.exists()
    assert metadata_path.exists()
    assert not canonical_path.exists()

    with metadata_path.open(
        "r",
        encoding="utf-8",
    ) as file:
        metadata = json.load(file)

    assert metadata["result"]["raw_row_count"] == 0


def test_run_daily_data_pipeline_does_not_write_invalid_canonical_data(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    raw_data = _make_raw_data()

    raw_data[("SPY", "Volume")] = (
        raw_data[("SPY", "Volume")].astype(float)
    )

    raw_data.loc[
        pd.Timestamp("2026-08-24"),
        ("SPY", "Volume"),
    ] = 1000.5

    monkeypatch.setattr(
        pipeline_module,
        "download_daily_data",
        lambda symbol, start_date, end_date: raw_data,
    )

    monkeypatch.setattr(
        pipeline_module,
        "_utc_now",
        lambda: datetime(
            2026,
            9,
            13,
            20,
            30,
            tzinfo=UTC,
        ),
    )

    raw_path = tmp_path / "raw" / "spy.csv"
    metadata_path = tmp_path / "raw" / "spy.metadata.json"
    canonical_path = tmp_path / "canonical" / "spy.csv"

    with pytest.raises(
        ValueError,
        match="Canonical volume must contain integer values",
    ):
        run_daily_data_pipeline(
            symbol="SPY",
            start_date=date(2026, 8, 24),
            end_date=date(2026, 8, 26),
            raw_path=raw_path,
            metadata_path=metadata_path,
            canonical_path=canonical_path,
        )

    assert raw_path.exists()
    assert metadata_path.exists()
    assert not canonical_path.exists()


def test_run_daily_data_pipeline_writes_nothing_when_download_fails(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    def failing_download(
        symbol: str,
        start_date: date,
        end_date: date,
    ) -> pd.DataFrame:
        raise RuntimeError(
            "Download failed"
        )

    monkeypatch.setattr(
        pipeline_module,
        "download_daily_data",
        failing_download,
    )

    raw_path = tmp_path / "raw" / "spy.csv"
    metadata_path = tmp_path / "raw" / "spy.metadata.json"
    canonical_path = tmp_path / "canonical" / "spy.csv"

    with pytest.raises(
        RuntimeError,
        match="Download failed",
    ):
        run_daily_data_pipeline(
            symbol="SPY",
            start_date=date(2026, 8, 24),
            end_date=date(2026, 8, 26),
            raw_path=raw_path,
            metadata_path=metadata_path,
            canonical_path=canonical_path,
        )

    assert not raw_path.exists()
    assert not metadata_path.exists()
    assert not canonical_path.exists()


def test_run_daily_data_pipeline_does_not_overwrite_existing_raw_file(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    raw_data = _make_raw_data()

    monkeypatch.setattr(
        pipeline_module,
        "download_daily_data",
        lambda symbol, start_date, end_date: raw_data,
    )

    raw_path = tmp_path / "raw" / "spy.csv"
    metadata_path = tmp_path / "raw" / "spy.metadata.json"
    canonical_path = tmp_path / "canonical" / "spy.csv"

    raw_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    raw_path.write_text(
        "existing raw evidence\n",
        encoding="utf-8",
    )

    with pytest.raises(FileExistsError):
        run_daily_data_pipeline(
            symbol="SPY",
            start_date=date(2026, 8, 24),
            end_date=date(2026, 8, 26),
            raw_path=raw_path,
            metadata_path=metadata_path,
            canonical_path=canonical_path,
        )

    assert raw_path.read_text(
        encoding="utf-8"
    ) == "existing raw evidence\n"

    assert not metadata_path.exists()
    assert not canonical_path.exists()