from datetime import UTC, date, datetime, timedelta, timezone

import backtester_data.acquisition as acquisition
import pandas as pd
import pytest


def test_download_daily_data_calls_yfinance_with_exact_configuration(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    expected_data = pd.DataFrame(
        {
            "value": [1, 2, 3],
        }
    )

    captured_kwargs: dict[str, object] = {}

    def fake_download(**kwargs: object) -> pd.DataFrame:
        captured_kwargs.update(kwargs)
        return expected_data

    monkeypatch.setattr(
        acquisition.yf,
        "download",
        fake_download,
    )

    result = acquisition.download_daily_data(
        symbol="SPY",
        start_date=date(2026, 1, 1),
        end_date=date(2026, 2, 1),
    )

    assert result is expected_data

    assert captured_kwargs == {
        "tickers": ["SPY"],
        "start": "2026-01-01",
        "end": "2026-02-01",
        "interval": "1d",
        "group_by": "ticker",
        "auto_adjust": False,
        "back_adjust": False,
        "repair": False,
        "keepna": True,
        "actions": True,
        "threads": False,
        "ignore_tz": True,
        "prepost": False,
        "rounding": False,
        "progress": False,
        "multi_level_index": True,
    }


@pytest.mark.parametrize(
    "symbol",
    [
        "",
        "   ",
        " SPY",
        "SPY ",
    ],
)
def test_download_daily_data_rejects_invalid_symbol(
    symbol: str,
) -> None:
    with pytest.raises(ValueError):
        acquisition.download_daily_data(
            symbol=symbol,
            start_date=date(2026, 1, 1),
            end_date=date(2026, 2, 1),
        )


@pytest.mark.parametrize(
    ("start_date", "end_date"),
    [
        (
            date(2026, 1, 1),
            date(2026, 1, 1),
        ),
        (
            date(2026, 2, 1),
            date(2026, 1, 1),
        ),
    ],
)
def test_download_daily_data_rejects_invalid_date_range(
    start_date: date,
    end_date: date,
) -> None:
    with pytest.raises(
        ValueError,
        match="Start date must be earlier than end date",
    ):
        acquisition.download_daily_data(
            symbol="SPY",
            start_date=start_date,
            end_date=end_date,
        )


def test_download_daily_data_wraps_provider_failure(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    provider_error = OSError("network failure")

    def fake_download(**kwargs: object) -> pd.DataFrame:
        del kwargs
        raise provider_error

    monkeypatch.setattr(
        acquisition.yf,
        "download",
        fake_download,
    )

    with pytest.raises(
        RuntimeError,
        match="Failed to download Yahoo Finance data for 'SPY'",
    ) as error_info:
        acquisition.download_daily_data(
            symbol="SPY",
            start_date=date(2026, 1, 1),
            end_date=date(2026, 2, 1),
        )

    assert error_info.value.__cause__ is provider_error


def test_download_daily_data_rejects_none_result(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    def fake_download(**kwargs: object) -> None:
        del kwargs
        return None

    monkeypatch.setattr(
        acquisition.yf,
        "download",
        fake_download,
    )

    with pytest.raises(
        RuntimeError,
        match="Yahoo Finance returned no dataset for 'SPY'",
    ):
        acquisition.download_daily_data(
            symbol="SPY",
            start_date=date(2026, 1, 1),
            end_date=date(2026, 2, 1),
        )


def test_download_daily_data_preserves_empty_dataframe(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    empty_data = pd.DataFrame()

    def fake_download(**kwargs: object) -> pd.DataFrame:
        del kwargs
        return empty_data

    monkeypatch.setattr(
        acquisition.yf,
        "download",
        fake_download,
    )

    result = acquisition.download_daily_data(
        symbol="SPY",
        start_date=date(2026, 1, 1),
        end_date=date(2026, 2, 1),
    )

    assert result is empty_data
    assert result.empty


def test_build_acquisition_metadata_records_exact_configuration(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(
        acquisition,
        "package_version",
        lambda package_name: "1.2.3",
    )

    raw_data = pd.DataFrame(
        {
            "value": [1, 2, 3],
        }
    )

    downloaded_at = datetime(
        2026,
        9,
        13,
        18,
        30,
        tzinfo=UTC,
    )

    metadata = acquisition.build_acquisition_metadata(
        symbol="SPY",
        start_date=date(2026, 1, 1),
        end_date=date(2026, 2, 1),
        raw_data=raw_data,
        downloaded_at_utc=downloaded_at,
    )

    assert metadata == {
        "provider": "yahoo_finance",
        "library": {
            "name": "yfinance",
            "version": "1.2.3",
        },
        "downloaded_at_utc": "2026-09-13T18:30:00+00:00",
        "request": {
            "symbols": ["SPY"],
            "selected_symbol": "SPY",
            "start_date": "2026-01-01",
            "end_date": "2026-02-01",
            "interval": "1d",
            "group_by": "ticker",
            "auto_adjust": False,
            "back_adjust": False,
            "repair": False,
            "keepna": True,
            "actions": True,
            "threads": False,
            "ignore_tz": True,
            "prepost": False,
            "rounding": False,
            "progress": False,
            "multi_level_index": True,
        },
        "result": {
            "raw_row_count": 3,
        },
    }


def test_build_acquisition_metadata_rejects_naive_timestamp() -> None:
    raw_data = pd.DataFrame()

    naive_timestamp = datetime(
        2026,
        9,
        13,
        18,
        30,
    )

    with pytest.raises(
        ValueError,
        match="Download timestamp must be timezone-aware",
    ):
        acquisition.build_acquisition_metadata(
            symbol="SPY",
            start_date=date(2026, 1, 1),
            end_date=date(2026, 2, 1),
            raw_data=raw_data,
            downloaded_at_utc=naive_timestamp,
        )


def test_build_acquisition_metadata_rejects_non_utc_timestamp() -> None:
    raw_data = pd.DataFrame()

    non_utc_timestamp = datetime(
        2026,
        9,
        13,
        15,
        30,
        tzinfo=timezone(timedelta(hours=-3)),
    )

    with pytest.raises(
        ValueError,
        match="Download timestamp must be in UTC",
    ):
        acquisition.build_acquisition_metadata(
            symbol="SPY",
            start_date=date(2026, 1, 1),
            end_date=date(2026, 2, 1),
            raw_data=raw_data,
            downloaded_at_utc=non_utc_timestamp,
        )